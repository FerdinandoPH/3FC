#include "debugInput.h"

#ifdef DEBUG_INPUT

#include "app.h"
#include "ftp/session.h"
#include "platform.h"
#include "ui.h"

#include <3ds.h>

#include <cstdio>
#include <cstring>

namespace
{
/// Every button that has been seen pressed since boot, OR'd together. If this
/// stays at zero while buttons are being mashed, HID is the problem and not
/// anything above it.
std::uint32_t s_seen  = 0;
/// Same idea one and two layers up: what platform::loop latched, and what
/// app::frame forwarded. Comparing the three sticky masks in a single glance
/// says whether a press was lost between HID and the screen — the live values
/// cannot, because a held button reads as zero in "down".
std::uint32_t s_seenPlat = 0;
std::uint32_t s_seenSent = 0;
unsigned s_pressCount = 0;
unsigned s_frames     = 0;

struct Named
{
	std::uint32_t mask;
	char const *name;
};

constexpr Named BUTTONS[] = {
    {KEY_A, "A"},
    {KEY_B, "B"},
    {KEY_X, "X"},
    {KEY_Y, "Y"},
    {KEY_L, "L"},
    {KEY_R, "R"},
    {KEY_START, "START"},
    {KEY_SELECT, "SEL"},
    {KEY_DUP, "UP"},
    {KEY_DDOWN, "DN"},
    {KEY_DLEFT, "LF"},
    {KEY_DRIGHT, "RT"},
};

void appendNames (char *const buffer, std::size_t const size, std::uint32_t const mask)
{
	buffer[0] = '\0';

	for (auto const &button : BUTTONS)
	{
		if (!(mask & button.mask))
			continue;

		if (buffer[0])
			std::strncat (buffer, "+", size - std::strlen (buffer) - 1);

		std::strncat (buffer, button.name, size - std::strlen (buffer) - 1);
	}

	if (!buffer[0])
		std::snprintf (buffer, size, "-");
}
}

void ui::drawInputDebug (std::uint32_t const dispatched)
{
	++s_frames;

	// Read HID directly as well: hidScanInput latches for the whole frame, so
	// this is the same data platform::loop saw, straight from the source.
	auto const rawDown = hidKeysDown ();
	auto const rawHeld = hidKeysHeld ();

	if (rawDown)
	{
		s_seen |= rawDown;
		++s_pressCount;
	}

	s_seenPlat |= platform::buttonsDown ();
	s_seenSent |= dispatched;

	auto const &st = app::state ();

	char held[96], seen[96], seenPlat[96], seenSent[96];
	appendNames (held, sizeof (held), rawHeld);
	appendNames (seen, sizeof (seen), s_seen);
	appendNames (seenPlat, sizeof (seenPlat), s_seenPlat);
	appendNames (seenSent, sizeof (seenSent), s_seenSent);

	char lines[5][128];
	std::snprintf (lines[0],
	    sizeof (lines[0]),
	    "held:%s  presses=%u  frames=%u",
	    held,
	    s_pressCount,
	    s_frames);
	std::snprintf (lines[1], sizeof (lines[1]), "hid  :%s", seen);
	std::snprintf (lines[2], sizeof (lines[2]), "plat :%s", seenPlat);
	std::snprintf (lines[3], sizeof (lines[3]), "sent :%s", seenSent);
	std::snprintf (lines[4],
	    sizeof (lines[4]),
	    "foc=%d scr=%d tab=%d cfm=%d menu=%d sess=%d",
	    static_cast<int> (st.focus),
	    static_cast<int> (st.screen),
	    st.bottomTab,
	    st.confirm.active () ? 1 : 0,
	    st.startMenu ? 1 : 0,
	    static_cast<int> (session::state ()));

	// Foreground draw list so no window can cover it.
	auto *const drawList = ImGui::GetForegroundDrawList ();
	auto const lineH     = ImGui::GetTextLineHeight ();
	auto const top       = TOP_Y + TOP_H - lineH * 5.0f - 2.0f;

	drawList->AddRectFilled (
	    ImVec2 (TOP_X, top), ImVec2 (TOP_X + TOP_W, TOP_Y + TOP_H), IM_COL32 (0, 0, 0, 210));

	for (int i = 0; i < 5; ++i)
	{
		drawList->AddText (ImVec2 (TOP_X + 2.0f, top + 1.0f + i * lineH),
		    IM_COL32 (255, 230, 120, 255),
		    lines[i]);
	}
}

#endif
