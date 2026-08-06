#pragma once

#include "fs/entry.h"

#include <cstdint>
#include <set>
#include <string>

namespace ui
{
/// Files and folders marked with SELECT, always belonging to one directory on
/// one machine. Selecting somewhere else replaces the whole thing, which is why
/// the spec asks for a warning first.
struct Selection
{
	fs::Machine machine = fs::LOCAL;
	std::string dir;
	std::set<std::string> names;

	bool empty () const
	{
		return names.empty ();
	}

	void clear ()
	{
		names.clear ();
		dir.clear ();
	}

	bool holds (fs::Machine const m, std::string const &d) const
	{
		return !names.empty () && machine == m && dir == d;
	}
};

/// \brief Draw the dual file explorer.
/// \param keys Buttons pressed this frame, or 0 when something else owns input
void drawBrowserScreen (std::uint32_t keys);

/// \brief Which side the explorer is showing, for the status bar.
fs::Machine browserMachine ();

/// \brief Reset the explorer, called when a new session starts.
void resetBrowser ();

/// \brief The current selection, shared with the actions menu.
Selection &selection ();

/// \brief Directory the given side is showing.
std::string const &browserPath (fs::Machine machine);

/// \brief Entry under the cursor on the active side, or nullptr if the list is
/// empty. Points into the pane's listing, so use it before anything refreshes.
fs::Entry const *browserCursorEntry ();

/// \brief Whether the active side already contains an entry with this name.
/// Used to decide whether an overwrite prompt is needed.
bool browserHasEntry (std::string const &name);

/// \brief Re-read a side's directory after it has been modified.
void refreshBrowser (fs::Machine machine);
}
