#include "app.h"

#include "i18n/lang.h"
#include "log.h"
#include "platform.h"
#include "ftp/session.h"
#include "transfer/queue.h"
#include "ui/debugInput.h"
#include "ui/screenActions.h"
#include "ui/screenBrowser.h"
#include "ui/screenConnect.h"

namespace
{
app::State s_state;

void drawTopScreen (std::uint32_t const keys)
{
	switch (app::state ().screen)
	{
	case app::SCREEN_CONNECT:
		ui::drawConnectScreen (keys);
		break;

	case app::SCREEN_BROWSER:
		ui::drawBrowserScreen (keys);
		break;
	}
}

void drawBottomScreen ()
{
	auto const &st = app::state ();

	ui::beginScreen ("##bottom", ui::BOT_X, ui::BOT_Y, ui::BOT_W, ui::BOT_H);

	// tab strip: the focused screen is the one L/R acts on, so make it obvious
	// which window is current
	for (int i = 0; i < app::TAB_COUNT; ++i)
	{
		if (i)
			ImGui::SameLine ();

		auto const current = i == st.bottomTab;
		ImGui::PushStyleColor (ImGuiCol_Text,
		    ImGui::GetColorU32 (current ? ImGuiCol_Text : ImGuiCol_TextDisabled));
		ImGui::TextUnformatted (i == app::TAB_PROGRESS ? TR (TAB_PROGRESS) : TR (TAB_CONSOLE));
		ImGui::PopStyleColor ();
	}

	ImGui::Separator ();

	if (st.bottomTab == app::TAB_PROGRESS)
		ui::drawProgressPanel ();
	else
		ui::drawConsolePanel ();

	ui::endScreen ();
}
}

app::State &app::state ()
{
	return s_state;
}

bool app::transfersActive ()
{
	return transfer::active ();
}

void app::frame ()
{
	auto &st = s_state;

	auto const keys = platform::buttonsRepeat ();

	// A link that dropped under us takes the browser with it: every folder on
	// screen belongs to a server we can no longer reach, and acting on any of
	// it would only produce errors. Checked here rather than in the browser so
	// it happens whichever screen has the focus and whatever menu is open.
	if (st.screen == SCREEN_BROWSER && session::state () == session::LOST)
	{
		transfer::cancelAll ();
		session::disconnect ();
		ui::resetBrowser ();
		ui::closeActionsMenu ();

		st.startMenu = false;
		st.screen    = SCREEN_CONNECT;
		st.confirm.notify (TR (CONNECTION_LOST));
	}

	// Input ownership, highest priority first: a confirmation prompt, then the
	// START menu, then the focused screen. X and START are always ours, so the
	// screens never see them.
	auto const busy = st.confirm.active () || st.startMenu || ui::actionsMenuOpen ();
	auto const topKeys =
	    (busy || st.focus != FOCUS_TOP) ? 0u : (keys & ~static_cast<std::uint32_t> (KEY_X | KEY_START));

	// The status bar is always visible; its right-hand text depends on the mode.
	static std::string status;
	if (st.screen == SCREEN_CONNECT)
		status = platform::localAddress ();
	else
	{
		status = session::description () + "  [" +
		         (ui::browserMachine () == fs::LOCAL ? TR (MACHINE_LOCAL) : TR (MACHINE_REMOTE)) +
		         "]";
	}
	ui::drawTopBar (status.c_str ());

	drawTopScreen (topKeys);
	drawBottomScreen ();

	ui::drawFocusIndicator (st.focus == FOCUS_TOP);

#ifdef DEBUG_INPUT
	ui::drawInputDebug (topKeys);
#endif

	if (st.confirm.draw (keys, st.focus == FOCUS_TOP))
		return;

	if (st.startMenu)
	{
		ui::drawStartMenu ();
		return;
	}

	if (keys & KEY_START)
	{
		ui::closeActionsMenu ();
		st.startMenu   = true;
		st.startCursor = 0;
		return;
	}

	if (ui::actionsMenuOpen ())
	{
		ui::drawActionsMenu (keys);
		return;
	}

	if (keys & KEY_X)
		st.focus = st.focus == FOCUS_TOP ? FOCUS_BOTTOM : FOCUS_TOP;
	else if (st.focus == FOCUS_BOTTOM)
	{
		if (keys & KEY_L)
			st.bottomTab = (st.bottomTab + TAB_COUNT - 1) % TAB_COUNT;
		if (keys & KEY_R)
			st.bottomTab = (st.bottomTab + 1) % TAB_COUNT;
	}
}
