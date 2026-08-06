#pragma once

#include <imgui.h>

#include <cstdint>
#include <functional>
#include <string>

namespace ui
{
/// Both screens live in one 400x480 ImGui viewport (see platform.cpp).
constexpr float SCREEN_W = 400.0f;
constexpr float TOP_X = 0.0f, TOP_Y = 0.0f, TOP_W = 400.0f, TOP_H = 240.0f;
constexpr float BOT_X = 40.0f, BOT_Y = 240.0f, BOT_W = 320.0f, BOT_H = 240.0f;

/// Height of the always-visible status strip at the top of the top screen.
constexpr float TOPBAR_H = 16.0f;

/// \brief Begin a borderless window pinned to a screen.
void beginScreen (char const *id, float x, float y, float w, float h);
void endScreen ();

/// \brief Move a list cursor with the d-pad/circle pad, wrapping around.
/// \param cursor Current index
/// \param count Number of items (0 returns 0)
/// \returns the new index
int moveCursor (int cursor, int count, std::uint32_t keysDown);

/// \brief Draw one list row, highlighting it when it is under the cursor.
/// \param marked Draw the label in the "selected for an operation" color
void row (char const *text, bool cursor, bool marked = false);

/// \brief Draw text clipped to \a width, scrolling it back and forth when
/// \a active and it does not fit, with an ellipsis otherwise.
void scrollingText (char const *text, float width, bool active, bool marked = false);

/// \brief A yes/no (optionally yes-to-all/no-to-all) prompt drawn over the top
/// screen. While active it swallows all input.
class Confirm
{
public:
	/// Answer passed to the callback.
	enum Answer
	{
		YES,
		NO,
		YES_ALL,
		NO_ALL,
	};

	using Callback = std::function<void (Answer)>;

	/// \brief Open a plain yes/no prompt.
	void ask (std::string message, Callback callback);

	/// \brief Open an acknowledge-only prompt (single OK button).
	void notify (std::string message);

	/// \brief Open a prompt that also offers "yes to all" / "no to all".
	/// Used by the overwrite prompt, where a recursive copy would otherwise
	/// ask dozens of times.
	void askAll (std::string message, Callback callback);

	bool active () const
	{
		return m_active;
	}

	/// \brief Draw and handle input.
	/// \param topScreen Which screen to appear on. It has to be the focused one:
	///        the other is dimmed by drawFocusIndicator, and a prompt asking for
	///        an answer must not be the faded thing on screen.
	/// \returns true if the prompt consumed this frame's input
	bool draw (std::uint32_t keysDown, bool topScreen);

private:
	bool m_active = false;
	bool m_all    = false;
	bool m_notify = false;
	/// Set by ask(); makes the first draw() ignore input, so the button press
	/// that opened the prompt does not also answer it.
	bool m_opened = false;
	int m_cursor  = 0;
	std::string m_message;
	Callback m_callback;
};

/// \brief Make it unmistakable which screen the buttons act on: a bright frame
/// around the focused one and a veil over the other. Without this the two
/// screens look identical and X appears to do nothing.
void drawFocusIndicator (bool topFocused);

/// Screen-level draw entry points.
void drawTopBar (char const *modeText);
void drawStartMenu ();
void drawConsolePanel ();
void drawProgressPanel ();
}
