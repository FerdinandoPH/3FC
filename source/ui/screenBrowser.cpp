#include "screenBrowser.h"

#include "app.h"
#include "fs/localFs.h"
#include "ftp/session.h"
#include "i18n/lang.h"
#include "log.h"
#include "screenActions.h"
#include "transfer/queue.h"
#include "ui.h"

#include <3ds.h>

#include <algorithm>
#include <cstdio>

namespace
{
/// Width reserved on the right of every row for the size column.
constexpr float SIZE_COLUMN = 62.0f;

struct Pane
{
	std::string path;
	fs::Listing listing;
	int cursor      = 0;
	bool needsFetch = true;
	bool scrollTo   = false;
};

fs::Machine s_machine = fs::LOCAL;
Pane s_panes[2];

Pane &pane (fs::Machine const machine)
{
	return s_panes[machine];
}

/// \brief Put the synthetic ".." row at the top of a pane.
///
/// Done here rather than in localFs/ftpClient so that it is present even when
/// the folder is empty or the listing failed outright — without a way back up,
/// a directory the user cannot leave is a dead end.
void addParentRow (Pane &p)
{
	if (fs::isRoot (p.path))
		return;

	p.listing.insert (p.listing.begin (), fs::Entry{"..", fs::PARENT, 0});
}

void refreshLocal ()
{
	auto &p = pane (fs::LOCAL);

	if (p.path.empty ())
		p.path = fs::local::ROOT;

	// SD reads are quick enough to do inline; only the network goes off-thread.
	fs::local::list (p.path, p.listing);
	addParentRow (p);

	p.cursor     = 0;
	p.scrollTo   = true;
	p.needsFetch = false;
}

void refreshRemote ()
{
	auto &p = pane (fs::REMOTE);

	if (p.path.empty ())
		p.path = "/";

	session::requestList (p.path);
	p.needsFetch = false;
}

/// Generation of the last remote listing copied out of the worker, so the
/// vector is copied once per arrival rather than once per frame.
unsigned s_seenGeneration = 0;

bool s_wasTransferring = false;

void syncAfterTransfers ()
{
	auto const busy = transfer::active ();

	if (s_wasTransferring && !busy)
	{
		s_panes[fs::LOCAL].needsFetch  = true;
		s_panes[fs::REMOTE].needsFetch = true;
	}

	s_wasTransferring = busy;
}

void syncRemote ()
{
	auto const generation = session::listGeneration ();
	if (generation == s_seenGeneration)
		return;

	s_seenGeneration = generation;

	std::string path;
	fs::Listing listing;
	session::listing (path, listing);

	auto &p = pane (fs::REMOTE);
	if (path != p.path)
		return; // a listing for a directory we have since navigated away from

	p.listing  = std::move (listing);
	addParentRow (p);
	p.cursor   = 0;
	p.scrollTo = true;
}

void navigateTo (fs::Machine const machine, std::string path)
{
	auto &p = pane (machine);

	p.path = std::move (path);

	// Drop the old rows now: a remote listing takes a round trip to arrive, and
	// until then they would be the previous folder's contents under the new path.
	p.listing.clear ();
	addParentRow (p);

	p.cursor     = 0;
	p.scrollTo   = true;
	p.needsFetch = true;
}

/// \brief Go up one level, if there is one.
void navigateUp (fs::Machine const machine)
{
	auto &p = pane (machine);

	if (fs::isRoot (p.path))
		return;

	navigateTo (machine, fs::parent (p.path));
}

ui::Selection s_selection;

void drawRow (fs::Entry const &entry, bool const cursor, bool const marked)
{
	auto const width  = ImGui::GetContentRegionAvail ().x;
	auto const height = ImGui::GetTextLineHeight ();
	auto const start  = ImGui::GetCursorScreenPos ();

	if (cursor)
	{
		ImGui::GetWindowDrawList ()->AddRectFilled (start,
		    ImVec2 (start.x + width, start.y + height),
		    ImGui::GetColorU32 (ImGuiCol_Header));
	}

	// Directories get a trailing slash so they read as directories even when
	// the name is clipped.
	char label[320];
	std::snprintf (label,
	    sizeof (label),
	    "%s%s",
	    entry.name.c_str (),
	    entry.type == fs::DIRECTORY ? "/" : "");

	ui::scrollingText (label, width - SIZE_COLUMN, cursor, marked);

	if (entry.type == fs::FILE)
	{
		auto const size = fs::formatSize (entry.size);
		auto const textWidth = ImGui::CalcTextSize (size.c_str ()).x;

		ImGui::GetWindowDrawList ()->AddText (
		    ImVec2 (start.x + width - textWidth, start.y),
		    ImGui::GetColorU32 (ImGuiCol_TextDisabled),
		    size.c_str ());
	}
}

/// \brief SELECT semantics from the spec: toggling within the current folder,
/// but switching folder or machine throws the old selection away, with a
/// warning first.
void handleSelect (fs::Machine const machine, app::State &st)
{
	auto &p = pane (machine);

	if (p.cursor < 0 || p.cursor >= static_cast<int> (p.listing.size ()))
		return;

	auto const &entry = p.listing[p.cursor];
	if (entry.type == fs::PARENT)
		return;

	auto const name = entry.name;

	if (s_selection.empty () || s_selection.holds (machine, p.path))
	{
		// Same folder: plain toggle, no warning needed.
		if (!s_selection.names.erase (name))
		{
			s_selection.machine = machine;
			s_selection.dir     = p.path;
			s_selection.names.insert (name);
		}
		return;
	}

	st.confirm.ask (TR (SELECTION_LOST), [machine, name, &st] (ui::Confirm::Answer const answer) {
		if (answer != ui::Confirm::YES)
			return;

		auto &target = pane (machine);
		s_selection.machine = machine;
		s_selection.dir     = target.path;
		s_selection.names.clear ();
		s_selection.names.insert (name);
	});
}

void handleActivate (fs::Machine const machine)
{
	auto &p = pane (machine);

	if (p.cursor < 0 || p.cursor >= static_cast<int> (p.listing.size ()))
		return;

	auto const &entry = p.listing[p.cursor];

	// Pressing A on a file does nothing, per the spec.
	if (entry.type == fs::FILE)
		return;

	if (entry.type == fs::PARENT)
		navigateUp (machine);
	else
		navigateTo (machine, fs::join (p.path, entry.name));
}
}

fs::Machine ui::browserMachine ()
{
	return s_machine;
}

void ui::resetBrowser ()
{
	s_machine        = fs::LOCAL;
	s_panes[0]       = Pane{};
	s_panes[1]       = Pane{};
	s_seenGeneration = session::listGeneration ();
	s_selection.clear ();
}

void ui::drawBrowserScreen (std::uint32_t const keys)
{
	auto &st = app::state ();

	syncRemote ();
	syncAfterTransfers ();

	auto &p = pane (s_machine);

	if (p.needsFetch)
	{
		if (s_machine == fs::LOCAL)
			refreshLocal ();
		else
			refreshRemote ();
	}

	beginScreen ("##browser", TOP_X, TOP_Y + TOPBAR_H, TOP_W, TOP_H - TOPBAR_H);

	// current directory
	ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
	scrollingText (p.path.c_str (), ImGui::GetContentRegionAvail ().x, true);
	ImGui::PopStyleColor ();
	ImGui::Separator ();

	ImGui::BeginChild ("##entries", ImVec2 (0, -ImGui::GetTextLineHeightWithSpacing ()));

	if (s_machine == fs::REMOTE && session::listPending ())
		ImGui::TextUnformatted (TR (LOADING));
	else if (p.listing.empty ())
		ImGui::TextUnformatted (TR (EMPTY_FOLDER));

	for (int i = 0; i < static_cast<int> (p.listing.size ()); ++i)
	{
		auto const marked = s_selection.holds (s_machine, p.path) &&
		                    s_selection.names.count (p.listing[i].name) > 0;
		drawRow (p.listing[i], i == p.cursor, marked);

		if (i == p.cursor && p.scrollTo)
		{
			ImGui::SetScrollHereY (0.5f);
			p.scrollTo = false;
		}
	}

	ImGui::EndChild ();

	ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
	ImGui::TextUnformatted (TR (HELP_HINT));
	ImGui::PopStyleColor ();

	endScreen ();

	if (!keys)
		return;

	// L/R swap explorers while the top screen has focus.
	if (keys & (KEY_L | KEY_R))
	{
		s_machine = s_machine == fs::LOCAL ? fs::REMOTE : fs::LOCAL;
		return;
	}

	// B is the same "go up" as activating ".."; disconnecting lives in the START
	// menu, where it cannot be triggered by mashing back out of a deep tree.
	if (keys & KEY_B)
	{
		navigateUp (s_machine);
		return;
	}

	auto const previous = p.cursor;
	p.cursor            = moveCursor (p.cursor, static_cast<int> (p.listing.size ()), keys);
	if (p.cursor != previous)
		p.scrollTo = true;

	if (keys & KEY_A)
		handleActivate (s_machine);
	else if (keys & KEY_SELECT)
		handleSelect (s_machine, st);
	else if (keys & KEY_Y)
		openActionsMenu ();
}

ui::Selection &ui::selection ()
{
	return s_selection;
}

std::string const &ui::browserPath (fs::Machine const machine)
{
	return pane (machine).path;
}

fs::Entry const *ui::browserCursorEntry ()
{
	auto const &p = pane (s_machine);

	if (p.cursor < 0 || p.cursor >= static_cast<int> (p.listing.size ()))
		return nullptr;

	return &p.listing[p.cursor];
}

bool ui::browserHasEntry (std::string const &name)
{
	auto const &p = pane (s_machine);

	return std::any_of (p.listing.begin (), p.listing.end (), [&name] (fs::Entry const &entry) {
		return entry.name == name;
	});
}

void ui::refreshBrowser (fs::Machine const machine)
{
	pane (machine).needsFetch = true;
}
