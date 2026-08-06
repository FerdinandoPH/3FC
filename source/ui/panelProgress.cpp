#include "ui.h"

#include "app.h"
#include "i18n/lang.h"
#include "platform.h"
#include "transfer/queue.h"

#include <3ds.h>

#include <algorithm>
#include <cstdio>

namespace
{
/// The job under the cursor, tracked by id rather than by row: the newest jobs
/// come in at the top, so every row's index shifts as the queue grows and an
/// index would silently start pointing at a different transfer.
unsigned s_cursorId = 0;

/// Set when the cursor moves, so the list scrolls to follow it exactly once
/// instead of being pinned every frame.
bool s_scrollTo = false;

char const *statusText (transfer::Status const status)
{
	switch (status)
	{
	case transfer::PENDING:
		return TR (STATUS_PENDING);
	case transfer::DONE:
		return TR (STATUS_DONE);
	case transfer::CANCELLED:
		return TR (STATUS_CANCELLED);
	case transfer::FAILED:
		return TR (STATUS_FAILED);
	case transfer::ACTIVE:
	default:
		return "";
	}
}

ImU32 statusColor (transfer::Status const status)
{
	switch (status)
	{
	case transfer::DONE:
		return IM_COL32 (140, 220, 140, 255);
	case transfer::FAILED:
		return IM_COL32 (240, 120, 120, 255);
	case transfer::CANCELLED:
		return IM_COL32 (200, 180, 120, 255);
	default:
		return ImGui::GetColorU32 (ImGuiCol_Text);
	}
}

/// \brief "1.2 MB/s - 00:42 left" for the active job.
void drawRate ()
{
	auto const rate = transfer::rate ();
	if (rate <= 0.0)
		return;

	auto const seconds = transfer::eta ();

	char buffer[64];
	if (seconds >= 0)
		std::snprintf (buffer,
		    sizeof (buffer),
		    "%s/s   %02d:%02d",
		    fs::formatSize (static_cast<std::uint64_t> (rate)).c_str (),
		    seconds / 60,
		    seconds % 60);
	else
		std::snprintf (
		    buffer, sizeof (buffer), "%s/s", fs::formatSize (static_cast<std::uint64_t> (rate)).c_str ());

	ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
	ImGui::TextUnformatted (buffer);
	ImGui::PopStyleColor ();
}

void drawJob (transfer::Job const &job, bool const cursor)
{
	auto const width = ImGui::GetContentRegionAvail ().x;

	ImGui::PushStyleColor (ImGuiCol_Text, statusColor (job.status));
	ui::row (job.name.c_str (), cursor);
	ImGui::PopStyleColor ();

	if (job.directory)
		return;

	if (job.status == transfer::ACTIVE)
	{
		auto const fraction =
		    job.size ? static_cast<float> (static_cast<double> (job.done) / job.size) : 0.0f;

		char overlay[48];
		std::snprintf (overlay,
		    sizeof (overlay),
		    "%s / %s",
		    fs::formatSize (job.done).c_str (),
		    fs::formatSize (job.size).c_str ());

		auto const barPos  = ImGui::GetCursorScreenPos ();
		auto const barSize = ImVec2 (width, ImGui::GetTextLineHeight ());

		// Empty overlay: ImGui pins its own to the right edge of the filled part,
		// so the numbers slide across the screen as the transfer advances and are
		// unreadable while they move. Centre them on the bar by hand instead.
		ImGui::ProgressBar (fraction, barSize, "");

		auto const textSize = ImGui::CalcTextSize (overlay);
		auto const textPos  = ImVec2 (barPos.x + (barSize.x - textSize.x) * 0.5f,
            barPos.y + (barSize.y - textSize.y) * 0.5f);

		auto *const drawList = ImGui::GetWindowDrawList ();
		drawList->AddText (textPos, ImGui::GetColorU32 (ImGuiCol_Text), overlay);

		// Light text disappears against the bar's fill, and now that the label no
		// longer runs away from it the two do overlap. Redraw just the part that
		// sits on the fill in dark ink.
		drawList->PushClipRect (barPos,
		    ImVec2 (barPos.x + barSize.x * fraction, barPos.y + barSize.y),
		    true);
		drawList->AddText (textPos, IM_COL32 (24, 24, 24, 255), overlay);
		drawList->PopClipRect ();

		drawRate ();
	}
	else
	{
		ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
		ImGui::Text ("%s  %s", fs::formatSize (job.size).c_str (), statusText (job.status));
		ImGui::PopStyleColor ();
	}
}
}

void ui::drawProgressPanel ()
{
	static std::vector<transfer::Job> jobs;
	transfer::snapshot (jobs);

	// The list is a history, not the queue: what is still waiting is already
	// summarised by the pending count above, and listing it would bury the one
	// transfer the user actually wants to watch.
	jobs.erase (std::remove_if (jobs.begin (),
	                jobs.end (),
	                [] (transfer::Job const &job) { return job.status == transfer::PENDING; }),
	    jobs.end ());

	// Jobs are appended in the order they run, so reversing puts the active one
	// on top and pushes each finished transfer down as the next one starts.
	std::reverse (jobs.begin (), jobs.end ());

	auto &st = app::state ();
	auto const count = static_cast<int> (jobs.size ());

	// Resolve the cursor against this frame's list. A job that is gone (the
	// queue was cleared) drops the cursor back to the top.
	int cursor    = 0;
	auto const it = std::find_if (jobs.begin (), jobs.end (), [] (transfer::Job const &job) {
		return job.id == s_cursorId;
	});

	if (it != jobs.end ())
		cursor = static_cast<int> (it - jobs.begin ());
	else if (!jobs.empty ())
		s_cursorId = jobs[0].id;

	if (transfer::expanding ())
		ImGui::TextUnformatted (TR (EXPANDING));

	// Read before the empty check: a fresh multi-file selection is a queue of
	// requests that have no jobs yet, and "no transfers" would be a lie.
	bool atLeast         = false;
	auto const remaining = transfer::pending (atLeast);

	if (remaining > 0 || !jobs.empty ())
	{
		ImGui::Text (atLeast ? TR (PENDING_COUNT_MIN) : TR (PENDING_COUNT), remaining);
		ImGui::Separator ();
	}

	if (jobs.empty ())
	{
		if (remaining == 0)
		{
			ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
			ImGui::TextUnformatted (TR (NO_TRANSFERS));
			ImGui::PopStyleColor ();
		}

		return;
	}

	ImGui::BeginChild ("##jobs", ImVec2 (0, -ImGui::GetTextLineHeightWithSpacing ()));

	for (int i = 0; i < count; ++i)
	{
		drawJob (jobs[i], i == cursor && st.focus == app::FOCUS_BOTTOM);

		if (i == cursor && s_scrollTo)
		{
			ImGui::SetScrollHereY (0.5f);
			s_scrollTo = false;
		}
	}

	ImGui::EndChild ();

	ImGui::PushStyleColor (ImGuiCol_Text, ImGui::GetColorU32 (ImGuiCol_TextDisabled));
	ImGui::TextUnformatted (TR (HELP_HINT));
	ImGui::PopStyleColor ();

	// Input only when the bottom screen has focus; L/R belong to the tab strip
	// and are handled by app::frame.
	if (st.focus != app::FOCUS_BOTTOM || st.confirm.active () || st.startMenu)
		return;

	auto const keys  = platform::buttonsRepeat ();
	auto const moved = moveCursor (cursor, count, keys);

	if (moved != cursor)
	{
		s_cursorId = jobs[moved].id;
		s_scrollTo = true;
	}

	if (keys & KEY_A)
	{
		// Only the active one is cancellable from here; the rest of the list has
		// already finished, and what is still queued is cancelled with Y.
		auto const &job = jobs[moved];
		if (job.status == transfer::ACTIVE)
		{
			auto const id = job.id;
			st.confirm.ask (TR (CANCEL_ONE), [id] (Confirm::Answer const answer) {
				if (answer == Confirm::YES)
					transfer::cancel (id);
			});
		}
	}
	else if (keys & KEY_Y)
	{
		// Held while the question is up, so the queue the user answers about is
		// the queue that gets cancelled.
		transfer::pause ();

		st.confirm.ask (TR (CANCEL_PENDING), [] (Confirm::Answer const answer) {
			if (answer == Confirm::YES)
				transfer::cancelPending ();

			transfer::resume ();
		});
	}
}
