#include "ui.h"

#include "app.h"
#include "ftp/session.h"
#include "i18n/lang.h"
#include "licenses.h"
#include "platform.h"
#include "screenBrowser.h"

namespace
{
enum Item
{
	ITEM_BACKLIGHT,
	ITEM_LANGUAGE,
	ITEM_DISCONNECT,
	ITEM_HELP,
	ITEM_LICENSES,
	ITEM_EXIT,
	ITEM_CLOSE,
	ITEM_COUNT,
};

bool s_licenses = false;
bool s_help     = false;

/// \brief Begin a full-screen reader on whichever screen has the focus, so it
/// is never the dimmed one.
///
/// Deliberately not ui::beginScreen: that one is for the windows that make up
/// the background, and a reader has to sit on top of them and hide them.
void beginReader (char const *const id, bool const topScreen)
{
	// The status bar is its own window over the top of the top screen, so start
	// below it rather than behind it.
	auto const inset = topScreen ? ui::TOPBAR_H : 0.0f;

	auto const x = topScreen ? ui::TOP_X : ui::BOT_X;
	auto const y = (topScreen ? ui::TOP_Y : ui::BOT_Y) + inset;
	auto const w = topScreen ? ui::TOP_W : ui::BOT_W;
	auto const h = (topScreen ? ui::TOP_H : ui::BOT_H) - inset;

	// Opaque, and brought to the front: the default window background is
	// translucent, so the browser underneath bled through and left the text
	// looking washed out over a screen it was supposed to replace.
	ImGui::SetNextWindowBgAlpha (1.0f);
	ImGui::SetNextWindowFocus ();
	ImGui::SetNextWindowPos (ImVec2 (x, y));
	ImGui::SetNextWindowSize (ImVec2 (w, h));
	ImGui::Begin (id,
	    nullptr,
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	        ImGuiWindowFlags_NoSavedSettings);
}

/// \brief Scroll the current child with the d-pad. There is nothing to select
/// in a reader, so the directions are free to move the text.
void scrollReader (std::uint32_t const keys)
{
	if (keys & (KEY_DUP | KEY_CPAD_UP))
		ImGui::SetScrollY (ImGui::GetScrollY () - 40.0f);
	if (keys & (KEY_DDOWN | KEY_CPAD_DOWN))
		ImGui::SetScrollY (ImGui::GetScrollY () + 40.0f);
	if (keys & (KEY_DLEFT | KEY_CPAD_LEFT))
		ImGui::SetScrollY (ImGui::GetScrollY () - ImGui::GetContentRegionAvail ().y);
	if (keys & (KEY_DRIGHT | KEY_CPAD_RIGHT))
		ImGui::SetScrollY (ImGui::GetScrollY () + ImGui::GetContentRegionAvail ().y);
}

/// \brief One "Top screen" / "Bottom screen" block of the controls reference.
void helpSection (char const *const title, char const *const body)
{
	// The heading carries the accent; the body stays full-strength text, because
	// this is the one screen the user came to read.
	ImGui::PushStyleColor (ImGuiCol_Text, IM_COL32 (120, 190, 255, 255));
	ImGui::TextUnformatted (title);
	ImGui::PopStyleColor ();

	ImGui::Separator ();
	ImGui::TextWrapped ("%s", body);
	ImGui::Spacing ();
}

/// \brief The controls reference. It lives here rather than as a hint line on
/// each screen because there are more buttons to explain than fit in one row.
/// \returns true while it is open and owning input
bool drawHelp (std::uint32_t const keys, bool const topScreen)
{
	if (!s_help)
		return false;

	beginReader ("##help", topScreen);

	ImGui::BeginChild ("##helptext");

	helpSection (TR (HELP_BOTH_TITLE), TR (HELP_BOTH));
	helpSection (TR (HELP_TOP_TITLE), TR (HELP_TOP));
	helpSection (TR (HELP_BOTTOM_TITLE), TR (HELP_BOTTOM));

	scrollReader (keys);

	ImGui::EndChild ();
	ui::endScreen ();

	if (keys & (KEY_B | KEY_A))
		s_help = false;

	return true;
}

/// \brief Full-screen scrollable attribution text.
/// \returns true while it is open and owning input
bool drawLicenses (std::uint32_t const keys, bool const topScreen)
{
	if (!s_licenses)
		return false;

	beginReader ("##licenses", topScreen);

	ImGui::BeginChild ("##licensetext");
	ImGui::TextWrapped ("%s", licenses::text ());

	scrollReader (keys);

	ImGui::EndChild ();
	ui::endScreen ();

	if (keys & (KEY_B | KEY_A))
		s_licenses = false;

	return true;
}
}

void ui::drawStartMenu ()
{
	auto &st = app::state ();

	// Everything this menu draws goes on the focused screen: the other one is
	// veiled, and a menu you are meant to answer must not be the faded one.
	auto const topScreen = st.focus == app::FOCUS_TOP;

	if (drawHelp (platform::buttonsRepeat (), topScreen))
		return;

	if (drawLicenses (platform::buttonsRepeat (), topScreen))
		return;

	// "Disconnect" only exists while a session is up, so the menu is built each
	// frame rather than being a fixed table indexed by Item.
	Item items[ITEM_COUNT];
	char const *labels[ITEM_COUNT];
	int count = 0;

	auto const add = [&items, &labels, &count] (Item const item, char const *const label) {
		items[count]  = item;
		labels[count] = label;
		++count;
	};

	add (ITEM_BACKLIGHT, TR (MENU_BACKLIGHT));
	add (ITEM_LANGUAGE, TR (MENU_LANGUAGE));
	if (st.screen == app::SCREEN_BROWSER)
		add (ITEM_DISCONNECT, TR (MENU_DISCONNECT));
	add (ITEM_HELP, TR (MENU_HELP));
	add (ITEM_LICENSES, TR (MENU_LICENSES));
	add (ITEM_EXIT, TR (MENU_EXIT));
	add (ITEM_CLOSE, TR (MENU_CLOSE));

	if (st.startCursor >= count)
		st.startCursor = 0;

	constexpr float W = 240.0f;

	auto const x = topScreen ? TOP_X : BOT_X;
	auto const y = topScreen ? TOP_Y : BOT_Y;
	auto const w = topScreen ? TOP_W : BOT_W;
	auto const h = topScreen ? TOP_H : BOT_H;

	// Auto-fit: the backlight hint appears and disappears with the cursor, so no
	// fixed height can be right for both states.
	ImGui::SetNextWindowSizeConstraints (ImVec2 (W, 0.0f), ImVec2 (W, h - 8.0f));
	ImGui::SetNextWindowPos (
	    ImVec2 (x + w * 0.5f, y + h * 0.5f), ImGuiCond_Always, ImVec2 (0.5f, 0.5f));
	ImGui::Begin ("##startmenu",
	    nullptr,
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::TextUnformatted (TR (MENU));
	ImGui::Separator ();

	for (int i = 0; i < count; ++i)
		row (labels[i], i == st.startCursor);

	if (items[st.startCursor] == ITEM_BACKLIGHT)
	{
		ImGui::Separator ();
		ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
		ImGui::TextWrapped ("%s", TR (MENU_BACKLIGHT_HINT));
		ImGui::PopStyleColor ();
	}

	ImGui::End ();

	auto const keys = platform::buttonsRepeat ();

	st.startCursor = moveCursor (st.startCursor, count, keys);

	if (keys & (KEY_B | KEY_START))
	{
		st.startMenu = false;
		return;
	}

	if (!(keys & KEY_A))
		return;

	switch (items[st.startCursor])
	{
	case ITEM_BACKLIGHT:
		st.startMenu = false;
		platform::backlightOff ();
		break;

	case ITEM_LANGUAGE:
		i18n::setLanguage (i18n::language () == i18n::ENGLISH ? i18n::SPANISH : i18n::ENGLISH);
		break;

	case ITEM_DISCONNECT:
		st.confirm.ask (TR (DISCONNECT_CONFIRM), [&st] (Confirm::Answer const answer) {
			st.startMenu = false;

			if (answer != Confirm::YES)
				return;

			session::disconnect ();
			resetBrowser ();
			st.screen = app::SCREEN_CONNECT;
		});
		break;

	case ITEM_HELP:
		s_help = true;
		break;

	case ITEM_LICENSES:
		s_licenses = true;
		break;

	case ITEM_EXIT:
		st.confirm.ask (app::transfersActive () ? TR (EXIT_CONFIRM_TRANSFERS) : TR (EXIT_CONFIRM),
		    [&st] (Confirm::Answer const answer) {
			    if (answer == Confirm::YES)
				    st.quit = true;
			    st.startMenu = false;
		    });
		break;

	case ITEM_CLOSE:
	case ITEM_COUNT:
		st.startMenu = false;
		break;
	}
}
