#pragma once

#include <cstdint>

namespace ui
{
/// \brief Draw the start-up screen (last connection / saved slots / editor).
/// \param keys Buttons pressed this frame, or 0 when something else owns input
void drawConnectScreen (std::uint32_t keys);
}
