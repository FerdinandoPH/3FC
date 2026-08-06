#include "ui.h"

#include "app.h"
#include "log.h"
#include "platform.h"

#include <3ds.h>

namespace
{
/// Rows moved per d-pad press; left/right jump a whole screenful.
constexpr float SCROLL_LINES = 3.0f;

ImU32 colorFor (logger::Level const level)
{
	switch (level)
	{
	case logger::SENT:
		return IM_COL32 (140, 200, 255, 255);
	case logger::RECEIVED:
		return IM_COL32 (180, 255, 180, 255);
	case logger::ERROR:
		return IM_COL32 (255, 120, 120, 255);
	case logger::DEBUG:
		return IM_COL32 (170, 170, 170, 255);
	case logger::INFO:
	default:
		return IM_COL32 (230, 230, 230, 255);
	}
}
}

void ui::drawConsolePanel ()
{
	// reused across frames so a 512-line log does not reallocate every frame
	static std::vector<logger::Line> lines;
	static std::size_t lastCount = 0;

	logger::snapshot (lines);

	auto const &st = app::state ();

	// The console owns the d-pad only while it is the focused screen, so the
	// browser above keeps its own navigation.
	auto const keys = (st.focus == app::FOCUS_BOTTOM && !st.confirm.active () && !st.startMenu)
	                      ? platform::buttonsRepeat ()
	                      : 0u;

	ImGui::BeginChild ("##console", ImVec2 (0, 0), ImGuiChildFlags_None);

	// Sampled before anything moves, and against last frame's extent, which is
	// the state the user was looking at when they pressed the button.
	auto const wasAtBottom = ImGui::GetScrollY () >= ImGui::GetScrollMaxY () - 1.0f;

	float delta = 0.0f;
	if (keys & (KEY_DUP | KEY_CPAD_UP))
		delta -= ImGui::GetTextLineHeightWithSpacing () * SCROLL_LINES;
	if (keys & (KEY_DDOWN | KEY_CPAD_DOWN))
		delta += ImGui::GetTextLineHeightWithSpacing () * SCROLL_LINES;
	if (keys & (KEY_DLEFT | KEY_CPAD_LEFT))
		delta -= ImGui::GetContentRegionAvail ().y;
	if (keys & (KEY_DRIGHT | KEY_CPAD_RIGHT))
		delta += ImGui::GetContentRegionAvail ().y;

	for (auto const &line : lines)
	{
		ImGui::PushStyleColor (ImGuiCol_Text, colorFor (line.level));
		ImGui::TextWrapped ("%s", line.text.c_str ());
		ImGui::PopStyleColor ();
	}

	if (delta != 0.0f)
		ImGui::SetScrollY (ImGui::GetScrollY () + delta);
	else if (lines.size () != lastCount && wasAtBottom)
	{
		// Follow the tail only from the tail: yanking the view back down while
		// the user is reading history is exactly what makes a log unusable.
		ImGui::SetScrollHereY (1.0f);
	}

	lastCount = lines.size ();

	ImGui::EndChild ();
}
