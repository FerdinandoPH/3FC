#include "ui.h"

#include "i18n/lang.h"
#include "platform.h"

#include <algorithm>
#include <cstdio>

namespace
{
/// Draw a battery pictogram with four discrete segments, plus the charge as
/// text. Vector shapes rather than a texture atlas, so the build needs no romfs
/// or tex3ds asset pipeline. Returns the horizontal space consumed.
float drawBattery (ImDrawList *const drawList, ImVec2 const pos, float const height)
{
	auto const percent  = platform::batteryPercent ();
	auto const charging = platform::batteryCharging ();

	auto const width   = height * 2.4f;
	auto const capW    = height * 0.16f;
	auto const outline = ImGui::GetColorU32 (ImGuiCol_Text);
	auto const empty   = ImGui::GetColorU32 (ImGuiCol_TextDisabled);

	// Shell: a heavier stroke than the 1px default, which all but vanished on
	// the 3DS screen.
	drawList->AddRect (pos,
	    ImVec2 (pos.x + width, pos.y + height),
	    outline,
	    0.0f,
	    ImDrawFlags_None,
	    1.5f);

	// Positive terminal.
	drawList->AddRectFilled (ImVec2 (pos.x + width, pos.y + height * 0.28f),
	    ImVec2 (pos.x + width + capW, pos.y + height * 0.72f),
	    outline);

	auto const fill = percent < 0    ? empty :
	                  charging       ? IM_COL32 (110, 220, 110, 255) :
	                  percent <= 15  ? IM_COL32 (235, 75, 75, 255) :
	                  percent <= 30  ? IM_COL32 (240, 180, 60, 255) :
	                                   outline;

	// Four segments inside the shell, each one an explicit slot so the level is
	// readable at a glance instead of being one anonymous bar.
	constexpr float PAD = 2.0f;
	constexpr float GAP = 1.0f;

	auto const segW = (width - 2.0f * PAD - 3.0f * GAP) / 4.0f;
	auto const top  = pos.y + PAD;
	auto const bot  = pos.y + height - PAD;

	for (int i = 0; i < 4; ++i)
	{
		auto const x0 = pos.x + PAD + i * (segW + GAP);
		auto const x1 = x0 + segW;

		// Slot outline: marks the segment even when it is empty.
		drawList->AddRect (ImVec2 (x0, top), ImVec2 (x1, bot), empty);

		if (percent < 0)
			continue;

		// Partial fill within the segment this level falls into.
		auto const level = (percent - i * 25.0f) / 25.0f;
		if (level <= 0.0f)
			continue;

		drawList->AddRectFilled (ImVec2 (x0, top),
		    ImVec2 (x0 + segW * std::min (level, 1.0f), bot),
		    fill);
	}

	char text[8];
	if (percent < 0)
		std::snprintf (text, sizeof (text), "--%%");
	else
		std::snprintf (text, sizeof (text), "%d%%", std::min (percent, 100));

	auto const textW = ImGui::CalcTextSize (text).x;
	auto const textX = pos.x + width + capW + 3.0f;
	auto const textY = pos.y + (height - ImGui::GetTextLineHeight ()) * 0.5f;
	drawList->AddText (ImVec2 (textX, textY), charging ? fill : outline, text);

	return width + capW + 3.0f + textW;
}

/// Draw a 3-bar wifi meter; empty bars are drawn dimmed so the widget keeps a
/// constant width.
void drawWifi (ImDrawList *const drawList, ImVec2 const pos, float const height)
{
	auto const strength = platform::wifiStrength ();
	auto const barW     = height * 0.3f;
	auto const on       = ImGui::GetColorU32 (ImGuiCol_Text);
	auto const off      = ImGui::GetColorU32 (ImGuiCol_TextDisabled);

	for (int i = 0; i < 3; ++i)
	{
		auto const h = height * (0.4f + 0.3f * i);
		auto const x = pos.x + i * (barW + 1.0f);
		drawList->AddRectFilled (ImVec2 (x, pos.y + height - h),
		    ImVec2 (x + barW, pos.y + height),
		    i < strength ? on : off);
	}
}
}

void ui::drawTopBar (char const *const modeText)
{
	ImGui::SetNextWindowPos (ImVec2 (TOP_X, TOP_Y));
	ImGui::SetNextWindowSize (ImVec2 (TOP_W, TOPBAR_H));
	ImGui::PushStyleVar (ImGuiStyleVar_WindowPadding, ImVec2 (4.0f, 2.0f));
	ImGui::Begin ("##topbar",
	    nullptr,
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
	        ImGuiWindowFlags_NoBringToFrontOnFocus);

	auto *const drawList = ImGui::GetWindowDrawList ();
	auto const height    = ImGui::GetTextLineHeight ();
	auto const origin    = ImGui::GetCursorScreenPos ();

	auto const iconH = height * 0.85f;
	auto const iconY = origin.y + (height - iconH) * 0.5f;

	auto const batteryW = drawBattery (drawList, ImVec2 (origin.x, iconY), iconH);

	auto const wifiX = origin.x + batteryW + 6.0f;
	drawWifi (drawList, ImVec2 (wifiX, iconY), iconH);

	ImGui::SetCursorPosX (
	    ImGui::GetCursorPosX () + (wifiX - origin.x) + iconH * 1.2f + 6.0f);
	ImGui::TextUnformatted (modeText ? modeText : TR (OFFLINE));

	ImGui::End ();
	ImGui::PopStyleVar ();
}
