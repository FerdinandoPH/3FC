#include "screenConnect.h"

#include "app.h"
#include "config/config.h"
#include "ftp/session.h"
#include "i18n/lang.h"
#include "log.h"
#include "platform.h"
#include "screenBrowser.h"
#include "swkbd.h"
#include "ui.h"

#include <cstdio>
#include <cstdlib>

namespace
{
enum Mode
{
	MODE_HOME,
	MODE_LIST,
	MODE_EDIT,
};

/// Rows of the slot editor, in display order.
enum Field
{
	FIELD_HOST,
	FIELD_PORT,
	FIELD_USER,
	FIELD_PASSWORD,
	FIELD_ANONYMOUS,
	FIELD_ALIAS,
	FIELD_SAVE,
	FIELD_DELETE,
	FIELD_COUNT,
};

Mode s_mode        = MODE_HOME;
int s_cursor       = 0;
int s_editingSlot  = -1;
config::Slot s_draft; ///< edited in place, only written back on Save

/// The home screen hides the "last used" entry when there is none, so the row
/// indices shift; resolve them through this.
bool hasLast ()
{
	return config::lastUsed () >= 0;
}

void openEditor (int const index)
{
	s_editingSlot = index;
	s_draft       = config::slot (index);
	s_mode        = MODE_EDIT;
	s_cursor      = 0;
}

void connectTo (int const index)
{
	auto const &slot = config::slot (index);
	if (slot.empty ())
		return;

	config::setLastUsed (index);
	config::save ();

	logger::printf (logger::INFO, TR (CONNECTING), slot.label ().c_str ());

	ui::resetBrowser ();
	session::connect (slot);
}

/// The connection runs on the worker, so the screen waits here for the result
/// instead of blocking.
void pollConnection ()
{
	auto &st = app::state ();

	switch (session::state ())
	{
	case session::CONNECTED:
		st.screen = app::SCREEN_BROWSER;
		break;

	case session::FAILED:
		st.confirm.notify (TR (CONNECT_FAILED));
		session::disconnect ();
		break;

	default:
		break;
	}
}

void drawHome (std::uint32_t const keys)
{
	auto const last  = config::lastUsed ();
	auto const count = hasLast () ? 2 : 1;

	if (hasLast ())
	{
		char buffer[128];
		std::snprintf (buffer,
		    sizeof (buffer),
		    "%s: %s",
		    TR (CONNECT_LAST),
		    config::slot (last).label ().c_str ());
		ui::row (buffer, s_cursor == 0);
	}

	ui::row (TR (CONNECT_SAVED), s_cursor == count - 1);

	s_cursor = ui::moveCursor (s_cursor, count, keys);

	if (!(keys & KEY_A))
		return;

	if (hasLast () && s_cursor == 0)
		connectTo (last);
	else
	{
		s_mode   = MODE_LIST;
		s_cursor = 0;
	}
}

void drawList (std::uint32_t const keys)
{
	for (int i = 0; i < config::SLOT_COUNT; ++i)
	{
		auto const &slot = config::slot (i);

		char buffer[160];
		std::snprintf (buffer,
		    sizeof (buffer),
		    "%d. %s",
		    i + 1,
		    slot.empty () ? TR (SLOT_EMPTY) : slot.label ().c_str ());

		ui::row (buffer, s_cursor == i);
	}

	ImGui::Separator ();
	ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
	ImGui::TextUnformatted (
	    config::slot (s_cursor).empty () ? TR (SLOT_HINT_EMPTY) : TR (SLOT_HINT_FILLED));
	ImGui::PopStyleColor ();

	s_cursor = ui::moveCursor (s_cursor, config::SLOT_COUNT, keys);

	if (keys & KEY_B)
	{
		s_mode   = MODE_HOME;
		s_cursor = 0;
		return;
	}

	if (keys & KEY_A)
	{
		if (config::slot (s_cursor).empty ())
			openEditor (s_cursor);
		else
			connectTo (s_cursor);
	}
	else if ((keys & KEY_Y) && !config::slot (s_cursor).empty ())
		openEditor (s_cursor);
}

void editField (Field const field)
{
	switch (field)
	{
	case FIELD_HOST:
		ui::swkbd::input (s_draft.host, TR (FIELD_HOST), s_draft.host, ui::swkbd::TEXT, 64);
		break;

	case FIELD_PORT:
	{
		std::string text = std::to_string (s_draft.port);
		if (ui::swkbd::input (text, TR (FIELD_PORT), text, ui::swkbd::NUMERIC, 5))
		{
			auto const port = std::atoi (text.c_str ());
			s_draft.port    = (port > 0 && port <= 65535) ? port : 21;
		}
		break;
	}

	case FIELD_USER:
		ui::swkbd::input (s_draft.user, TR (FIELD_USER), s_draft.user, ui::swkbd::TEXT, 64);
		break;

	case FIELD_PASSWORD:
		ui::swkbd::input (
		    s_draft.password, TR (FIELD_PASSWORD), s_draft.password, ui::swkbd::PASSWORD, 64);
		break;

	case FIELD_ANONYMOUS:
		s_draft.anonymous = !s_draft.anonymous;
		break;

	case FIELD_ALIAS:
		ui::swkbd::input (s_draft.alias, TR (FIELD_ALIAS), s_draft.alias, ui::swkbd::TEXT, 32);
		break;

	default:
		break;
	}
}

void drawEditor (std::uint32_t const keys)
{
	auto &st = app::state ();

	char buffer[160];

	auto const labelled = [&] (Field const field, char const *const label, char const *const value) {
		std::snprintf (buffer, sizeof (buffer), "%s: %s", label, value);
		ui::row (buffer, s_cursor == field);
	};

	labelled (FIELD_HOST, TR (FIELD_HOST), s_draft.host.c_str ());
	labelled (FIELD_PORT, TR (FIELD_PORT), std::to_string (s_draft.port).c_str ());

	// The anonymous checkbox supplies the credentials, so grey the two fields
	// it overrides instead of letting the user edit values that are ignored.
	ImGui::BeginDisabled (s_draft.anonymous);
	labelled (FIELD_USER, TR (FIELD_USER), s_draft.effectiveUser ().c_str ());
	labelled (FIELD_PASSWORD, TR (FIELD_PASSWORD), s_draft.password.empty () ? "" : "********");
	ImGui::EndDisabled ();

	labelled (FIELD_ANONYMOUS, TR (FIELD_ANONYMOUS), s_draft.anonymous ? "[x]" : "[ ]");
	labelled (FIELD_ALIAS, TR (FIELD_ALIAS), s_draft.alias.c_str ());

	ImGui::Separator ();
	ui::row (TR (SLOT_SAVE), s_cursor == FIELD_SAVE);
	ui::row (TR (SLOT_DELETE), s_cursor == FIELD_DELETE);

	if (s_cursor == FIELD_PASSWORD)
	{
		ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
		ImGui::TextWrapped ("%s", TR (PASSWORD_PLAINTEXT));
		ImGui::PopStyleColor ();
	}

	s_cursor = ui::moveCursor (s_cursor, FIELD_COUNT, keys);

	if (keys & KEY_B)
	{
		// discard the draft
		s_mode   = MODE_LIST;
		s_cursor = s_editingSlot;
		return;
	}

	if (!(keys & KEY_A))
		return;

	switch (s_cursor)
	{
	case FIELD_SAVE:
		if (s_draft.host.empty ())
		{
			st.confirm.notify (TR (SLOT_NEEDS_HOST));
			return;
		}

		config::slot (s_editingSlot) = s_draft;
		config::save ();
		s_mode   = MODE_LIST;
		s_cursor = s_editingSlot;
		break;

	case FIELD_DELETE:
		st.confirm.ask (TR (SLOT_DELETE_CONFIRM), [] (ui::Confirm::Answer const answer) {
			if (answer != ui::Confirm::YES)
				return;

			config::slot (s_editingSlot) = config::Slot{};
			config::save ();
			s_mode   = MODE_LIST;
			s_cursor = s_editingSlot;
		});
		break;

	default:
		editField (static_cast<Field> (s_cursor));
		break;
	}
}
}

void ui::drawConnectScreen (std::uint32_t const keys)
{
	pollConnection ();

	beginScreen ("##connect", TOP_X, TOP_Y + TOPBAR_H, TOP_W, TOP_H - TOPBAR_H);

	// While the worker is connecting there is nothing to choose, but B has to
	// keep working: a connection attempt to an unreachable host takes seconds,
	// and without a way out the app looks completely dead.
	if (session::state () == session::CONNECTING)
	{
		ImGui::TextUnformatted (TR (LOADING));
		ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
		ImGui::TextUnformatted (TR (CONNECT_ABORT_HINT));
		ImGui::PopStyleColor ();
		endScreen ();

		if (keys & KEY_B)
			session::disconnect ();

		return;
	}

	switch (s_mode)
	{
	case MODE_HOME:
		drawHome (keys);
		break;
	case MODE_LIST:
		drawList (keys);
		break;
	case MODE_EDIT:
		drawEditor (keys);
		break;
	}

	endScreen ();
}
