#include "ui.h"

#include "i18n/lang.h"
#include "platform.h"

#include <imgui_internal.h>

#include <cmath>
#include <cstring>

namespace
{
constexpr auto MARKED_COLOR = IM_COL32 (255, 210, 80, 255);

/// Pixels per second the marquee slides at, and how long it pauses at each end.
constexpr float SCROLL_SPEED = 30.0f;
constexpr float SCROLL_PAUSE = 0.75f;
}

void ui::beginScreen (char const *const id, float x, float y, float w, float h)
{
	ImGui::SetNextWindowPos (ImVec2 (x, y));
	ImGui::SetNextWindowSize (ImVec2 (w, h));
	ImGui::Begin (id,
	    nullptr,
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
	        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
}

void ui::endScreen ()
{
	ImGui::End ();
}

int ui::moveCursor (int cursor, int const count, std::uint32_t const keysDown)
{
	if (count <= 0)
		return 0;

	if (keysDown & (KEY_DUP | KEY_CPAD_UP))
		--cursor;
	if (keysDown & (KEY_DDOWN | KEY_CPAD_DOWN))
		++cursor;

	// a page is roughly a screenful of rows
	if (keysDown & (KEY_DLEFT | KEY_CPAD_LEFT))
		cursor -= 8;
	if (keysDown & (KEY_DRIGHT | KEY_CPAD_RIGHT))
		cursor += 8;

	if (cursor < 0)
		cursor = (keysDown & (KEY_DLEFT | KEY_CPAD_LEFT)) ? 0 : count - 1;
	else if (cursor >= count)
		cursor = (keysDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) ? count - 1 : 0;

	return cursor;
}

void ui::row (char const *const text, bool const cursor, bool const marked)
{
	auto const width = ImGui::GetContentRegionAvail ().x;

	if (cursor)
	{
		auto const pos = ImGui::GetCursorScreenPos ();
		ImGui::GetWindowDrawList ()->AddRectFilled (pos,
		    ImVec2 (pos.x + width, pos.y + ImGui::GetTextLineHeight ()),
		    ImGui::GetColorU32 (ImGuiCol_Header));
	}

	scrollingText (text, width, cursor, marked);
}

void ui::scrollingText (char const *const text,
    float const width,
    bool const active,
    bool const marked)
{
	auto const color   = marked ? MARKED_COLOR : ImGui::GetColorU32 (ImGuiCol_Text);
	auto const textEnd = text + std::strlen (text);
	auto const size    = ImGui::CalcTextSize (text);
	auto const pos     = ImGui::GetCursorScreenPos ();
	auto const height  = ImGui::GetTextLineHeight ();

	// reserve the row regardless of how we end up drawing the text
	ImGui::Dummy (ImVec2 (width, height));

	auto *const drawList = ImGui::GetWindowDrawList ();

	if (size.x <= width)
	{
		drawList->AddText (pos, color, text, textEnd);
		return;
	}

	if (!active)
	{
		// not under the cursor: clip with an ellipsis
		ImGui::RenderTextEllipsis (drawList,
		    pos,
		    ImVec2 (pos.x + width, pos.y + height),
		    pos.x + width,
		    pos.x + width,
		    text,
		    textEnd,
		    &size);
		return;
	}

	// under the cursor: slide the name back and forth so it can be read whole
	auto const overflow = size.x - width;
	auto const travel   = overflow / SCROLL_SPEED;
	auto const period   = 2.0f * (travel + SCROLL_PAUSE);
	auto const t        = std::fmod (ImGui::GetTime (), period);

	float offset;
	if (t < SCROLL_PAUSE)
		offset = 0.0f;
	else if (t < SCROLL_PAUSE + travel)
		offset = (t - SCROLL_PAUSE) * SCROLL_SPEED;
	else if (t < 2.0f * SCROLL_PAUSE + travel)
		offset = overflow;
	else
		offset = overflow - (t - 2.0f * SCROLL_PAUSE - travel) * SCROLL_SPEED;

	drawList->PushClipRect (pos, ImVec2 (pos.x + width, pos.y + height), true);
	drawList->AddText (ImVec2 (pos.x - offset, pos.y), color, text, textEnd);
	drawList->PopClipRect ();
}

void ui::drawFocusIndicator (bool const topFocused)
{
	constexpr auto ACCENT = IM_COL32 (120, 190, 255, 255);
	constexpr auto VEIL   = IM_COL32 (0, 0, 0, 110);

	auto const topMin    = ImVec2 (TOP_X, TOP_Y);
	auto const topMax    = ImVec2 (TOP_X + TOP_W, TOP_Y + TOP_H);
	auto const bottomMin = ImVec2 (BOT_X, BOT_Y);
	auto const bottomMax = ImVec2 (BOT_X + BOT_W, BOT_Y + BOT_H);

	// Foreground list so the frame sits over every window, including dialogs.
	auto *const drawList = ImGui::GetForegroundDrawList ();

	auto const &dimMin   = topFocused ? bottomMin : topMin;
	auto const &dimMax   = topFocused ? bottomMax : topMax;
	auto const &focusMin = topFocused ? topMin : bottomMin;
	auto const &focusMax = topFocused ? topMax : bottomMax;

	drawList->AddRectFilled (dimMin, dimMax, VEIL);

	// Inset by a pixel so the whole stroke stays on screen.
	drawList->AddRect (ImVec2 (focusMin.x + 1.0f, focusMin.y + 1.0f),
	    ImVec2 (focusMax.x - 1.0f, focusMax.y - 1.0f),
	    ACCENT,
	    0.0f,
	    0,
	    2.0f);
}

void ui::Confirm::ask (std::string message, Callback callback)
{
	m_active   = true;
	m_all      = false;
	m_notify   = false;
	m_opened   = true;
	m_cursor   = 0;
	m_message  = std::move (message);
	m_callback = std::move (callback);
}

void ui::Confirm::askAll (std::string message, Callback callback)
{
	ask (std::move (message), std::move (callback));
	m_all = true;
}

void ui::Confirm::notify (std::string message)
{
	ask (std::move (message), nullptr);
	m_notify = true;
}

bool ui::Confirm::draw (std::uint32_t const keysDown, bool const topScreen)
{
	if (!m_active)
		return false;

	char const *const options[] = {TR (YES), TR (NO), TR (YES_ALL), TR (NO_ALL)};
	char const *const ack[]     = {TR (OK)};

	auto const count = m_notify ? 1 : (m_all ? 4 : 2);

	constexpr float W = 260.0f;

	auto const x = topScreen ? TOP_X : BOT_X;
	auto const y = topScreen ? TOP_Y : BOT_Y;
	auto const w = topScreen ? TOP_W : BOT_W;
	auto const h = topScreen ? TOP_H : BOT_H;

	// Auto-fit: the message wraps to an unknown number of lines and there are
	// either one, two or four options underneath it.
	ImGui::SetNextWindowSizeConstraints (ImVec2 (W, 0.0f), ImVec2 (W, h - 8.0f));
	ImGui::SetNextWindowPos (
	    ImVec2 (x + w * 0.5f, y + h * 0.5f), ImGuiCond_Always, ImVec2 (0.5f, 0.5f));
	ImGui::Begin ("##confirm",
	    nullptr,
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::TextWrapped ("%s", m_message.c_str ());
	ImGui::Separator ();

	for (int i = 0; i < count; ++i)
		row (m_notify ? ack[i] : options[i], i == m_cursor);

	ImGui::End ();

	if (m_opened)
	{
		// swallow the press that opened this prompt
		m_opened = false;
		return true;
	}

	m_cursor = moveCursor (m_cursor, count, keysDown);

	if (keysDown & KEY_A)
	{
		auto const callback = m_callback;
		auto const answer   = static_cast<Answer> (m_cursor);

		m_active = false;
		m_callback = nullptr;

		if (callback)
			callback (answer);
	}
	else if (keysDown & KEY_B)
	{
		auto const callback = m_callback;

		m_active = false;
		m_callback = nullptr;

		if (callback)
			callback (NO);
	}

	return true;
}
