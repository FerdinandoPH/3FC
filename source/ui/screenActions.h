#pragma once

#include <cstdint>

namespace ui
{
/// \brief Open the Y menu for the side the explorer is showing.
void openActionsMenu ();

bool actionsMenuOpen ();

/// \brief Draw and drive the Y menu. Only called while it is open.
void drawActionsMenu (std::uint32_t keys);

void closeActionsMenu ();
}
