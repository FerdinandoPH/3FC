#include "screenActions.h"

#include "app.h"
#include "fs/ops.h"
#include "ftp/session.h"
#include "i18n/lang.h"
#include "log.h"
#include "screenBrowser.h"
#include "swkbd.h"
#include "transfer/queue.h"
#include "ui.h"

#include <3ds.h>

#include <cstdio>
#include <vector>

namespace
{
enum Action
{
	NEW_FOLDER,
	RENAME,
	DELETE_CURSOR,
	DELETE_SELECTED,
	PASTE,
	TRANSFER,
	CLOSE,
};

struct Item
{
	Action action;
	std::string label;
};

bool s_open   = false;
/// The Y press that opens the menu is still in this frame's key mask, and Y
/// also closes the menu; swallow it once.
bool s_justOpened = false;
int s_cursor  = 0;
std::vector<Item> s_items;

/// \brief Re-read the affected side once a worker operation has finished.
/// Called from the worker; needsFetch is a plain flag the UI polls.
void refreshLater (fs::Machine const machine)
{
	ui::refreshBrowser (machine);
}

/// \brief Build the menu from the spec's rules: what is offered depends on the
/// cursor, on whether there is a selection, and on where that selection lives.
void buildItems ()
{
	s_items.clear ();

	auto const machine   = ui::browserMachine ();
	auto const &path     = ui::browserPath (machine);
	auto const &selected = ui::selection ();
	auto const *entry    = ui::browserCursorEntry ();

	char buffer[192];

	s_items.push_back ({NEW_FOLDER, TR (ACTION_NEW_FOLDER)});

	// ".." is not a real entry, so it can be neither renamed nor deleted.
	if (entry && entry->type != fs::PARENT)
	{
		std::snprintf (buffer, sizeof (buffer), TR (ACTION_RENAME), entry->name.c_str ());
		s_items.push_back ({RENAME, buffer});

		std::snprintf (buffer, sizeof (buffer), TR (ACTION_DELETE), entry->name.c_str ());
		s_items.push_back ({DELETE_CURSOR, buffer});
	}

	if (!selected.empty ())
	{
		auto const count = static_cast<int> (selected.names.size ());

		if (selected.machine == machine)
		{
			if (selected.dir == path)
			{
				std::snprintf (buffer, sizeof (buffer), TR (ACTION_DELETE_SELECTED), count);
				s_items.push_back ({DELETE_SELECTED, buffer});
			}
			else if (machine == fs::LOCAL)
			{
				// Copying only exists on the SD card. FTP has no copy command,
				// and routing the bytes through the console to fake one is
				// deliberately not offered.
				std::snprintf (buffer, sizeof (buffer), TR (ACTION_PASTE), count);
				s_items.push_back ({PASTE, buffer});
			}
		}
		else
		{
			std::snprintf (buffer, sizeof (buffer), TR (ACTION_TRANSFER), count);
			s_items.push_back ({TRANSFER, buffer});
		}
	}

	s_items.push_back ({CLOSE, TR (ACTION_CLOSE)});
}

void doNewFolder ()
{
	auto const machine = ui::browserMachine ();
	auto const path    = ui::browserPath (machine);

	std::string name;
	if (!ui::swkbd::input (name, TR (PROMPT_NEW_FOLDER), "", ui::swkbd::TEXT, 64) || name.empty ())
		return;

	auto const target = fs::join (path, name);

	session::post ([machine, target] (ftp::Client &client) {
		fs::ops::makeDirectory (client, machine, target);
		refreshLater (machine);
	});
}

void doRename (std::string const &name)
{
	auto const machine = ui::browserMachine ();
	auto const path    = ui::browserPath (machine);

	std::string renamed;
	if (!ui::swkbd::input (renamed, TR (PROMPT_RENAME), name, ui::swkbd::TEXT, 128) ||
	    renamed.empty () || renamed == name)
		return;

	auto const from = fs::join (path, name);
	auto const to   = fs::join (path, renamed);

	session::post ([machine, from, to] (ftp::Client &client) {
		fs::ops::rename (client, machine, from, to);
		refreshLater (machine);
	});
}

void doDelete (std::vector<std::string> paths)
{
	auto const machine = ui::browserMachine ();

	session::post ([machine, paths] (ftp::Client &client) {
		for (auto const &path : paths)
			fs::ops::removeTree (client, machine, path);

		refreshLater (machine);
	});
}

/// \brief Queue the selection, once the overwrite question has been answered.
void queueSelection (transfer::Direction const direction, transfer::Overwrite const overwrite)
{
	auto const &selected = ui::selection ();
	auto const machine   = ui::browserMachine ();
	auto const destDir   = ui::browserPath (machine);

	for (auto const &name : selected.names)
	{
		transfer::Request request;
		request.direction = direction;
		request.source    = fs::join (selected.dir, name);
		request.destDir   = destDir;
		request.overwrite = overwrite;
		transfer::enqueue (request);
	}

	ui::selection ().clear ();
	ui::refreshBrowser (machine);
}

/// \brief Ask about overwriting only if something would actually be replaced.
/// The destination is the folder on screen, so its cached listing answers the
/// question without a single extra round trip.
void queueWithOverwriteCheck (transfer::Direction const direction, app::State &st)
{
	auto const &selected = ui::selection ();

	std::string clash;
	for (auto const &name : selected.names)
	{
		if (ui::browserHasEntry (name))
		{
			clash = name;
			break;
		}
	}

	if (clash.empty ())
	{
		queueSelection (direction, transfer::OVERWRITE_YES);
		return;
	}

	char message[192];
	std::snprintf (message, sizeof (message), TR (CONFIRM_OVERWRITE), clash.c_str ());

	st.confirm.askAll (message, [direction] (ui::Confirm::Answer const answer) {
		switch (answer)
		{
		case ui::Confirm::YES:
		case ui::Confirm::YES_ALL:
			queueSelection (direction, transfer::OVERWRITE_YES);
			break;

		case ui::Confirm::NO_ALL:
			// Queue everything, skipping whatever already exists.
			queueSelection (direction, transfer::OVERWRITE_NO);
			break;

		case ui::Confirm::NO:
			break;
		}
	});
}

void activate (Action const action, app::State &st)
{
	auto const machine   = ui::browserMachine ();
	auto const &path     = ui::browserPath (machine);
	auto const &selected = ui::selection ();
	auto const *entry    = ui::browserCursorEntry ();

	switch (action)
	{
	case NEW_FOLDER:
		ui::closeActionsMenu ();
		doNewFolder ();
		break;

	case RENAME:
		if (entry)
		{
			auto const name = entry->name;
			ui::closeActionsMenu ();
			doRename (name);
		}
		break;

	case DELETE_CURSOR:
		if (entry)
		{
			auto const target = fs::join (path, entry->name);

			char message[224];
			std::snprintf (message, sizeof (message), TR (CONFIRM_DELETE), entry->name.c_str ());

			ui::closeActionsMenu ();
			st.confirm.ask (message, [target] (ui::Confirm::Answer const answer) {
				if (answer == ui::Confirm::YES)
					doDelete ({target});
			});
		}
		break;

	case DELETE_SELECTED:
	{
		std::vector<std::string> targets;
		for (auto const &name : selected.names)
			targets.push_back (fs::join (selected.dir, name));

		char message[224];
		std::snprintf (
		    message, sizeof (message), TR (CONFIRM_DELETE_SELECTED), static_cast<int> (targets.size ()));

		ui::closeActionsMenu ();
		st.confirm.ask (message, [targets] (ui::Confirm::Answer const answer) {
			if (answer != ui::Confirm::YES)
				return;

			doDelete (targets);
			ui::selection ().clear ();
		});
		break;
	}

	case PASTE:
		ui::closeActionsMenu ();
		queueWithOverwriteCheck (transfer::LOCAL_COPY, st);
		break;

	case TRANSFER:
		ui::closeActionsMenu ();
		// The destination is the side on screen, so the direction follows from
		// where the selection came from.
		queueWithOverwriteCheck (
		    machine == fs::LOCAL ? transfer::DOWNLOAD : transfer::UPLOAD, st);
		break;

	case CLOSE:
		ui::closeActionsMenu ();
		break;
	}
}
}

void ui::openActionsMenu ()
{
	buildItems ();
	s_cursor     = 0;
	s_open       = true;
	s_justOpened = true;
}

bool ui::actionsMenuOpen ()
{
	return s_open;
}

void ui::closeActionsMenu ()
{
	s_open = false;
}

void ui::drawActionsMenu (std::uint32_t const keys)
{
	auto &st = app::state ();

	auto const count = static_cast<int> (s_items.size ());

	constexpr float W = 300.0f;

	// Auto-fit the height instead of computing it: a hand-rolled figure has to
	// account for the title, the separator, the window padding and the item
	// spacing of every row, and getting any of them wrong silently eats the
	// bottom entries. The pivot keeps it centred whatever height it lands on.
	ImGui::SetNextWindowSizeConstraints (ImVec2 (W, 0.0f), ImVec2 (W, TOP_H - 8.0f));
	ImGui::SetNextWindowPos (ImVec2 (TOP_X + TOP_W * 0.5f, TOP_Y + TOP_H * 0.5f),
	    ImGuiCond_Always,
	    ImVec2 (0.5f, 0.5f));
	ImGui::Begin ("##actions",
	    nullptr,
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::TextUnformatted (TR (ACTIONS));
	ImGui::Separator ();

	for (int i = 0; i < count; ++i)
		row (s_items[i].label.c_str (), i == s_cursor);

	ImGui::End ();

	if (s_justOpened)
	{
		s_justOpened = false;
		return;
	}

	s_cursor = moveCursor (s_cursor, count, keys);

	if (keys & (KEY_B | KEY_Y))
	{
		closeActionsMenu ();
		return;
	}

	if ((keys & KEY_A) && s_cursor >= 0 && s_cursor < count)
		activate (s_items[s_cursor].action, st);
}
