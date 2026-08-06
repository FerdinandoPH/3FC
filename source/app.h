#pragma once

#include "ui/ui.h"

namespace app
{
/// Which screen the buttons act on. Toggled with X.
enum Focus
{
	FOCUS_TOP,
	FOCUS_BOTTOM,
};

/// Bottom-screen windows, alternated with L/R when the bottom screen has focus.
enum BottomTab
{
	TAB_PROGRESS,
	TAB_CONSOLE,
	TAB_COUNT,
};

/// What the top screen is showing.
enum Screen
{
	SCREEN_CONNECT,
	SCREEN_BROWSER,
};

struct State
{
	Focus focus       = FOCUS_TOP;
	int bottomTab     = TAB_PROGRESS;
	Screen screen     = SCREEN_CONNECT;
	bool startMenu    = false;
	int startCursor   = 0;
	bool quit         = false;
	ui::Confirm confirm;
};

State &state ();

/// \brief Whether any transfer is still running or queued.
bool transfersActive ();

/// \brief Build one frame of UI and handle its input.
void frame ();
}
