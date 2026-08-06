#pragma once

#include <cstdint>

namespace ui
{
/// \brief Draw a button-state overlay on top of everything.
///
/// Only compiled in with EXTRA_DEFINES=-DDEBUG_INPUT (./run.sh -d). It exists
/// to tell apart the two ways input can be broken: HID never reporting a press
/// at all, versus the app's own dispatch swallowing it.
///
/// \param dispatched The mask app::frame actually forwarded to the top screen
void drawInputDebug (std::uint32_t dispatched);
}
