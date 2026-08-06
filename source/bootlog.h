#pragma once

namespace bootlog
{
/// Start-up breadcrumbs written to sdmc:/3ds/3FC/boot.log, flushed after
/// every entry.
///
/// On hardware there is no console and no debugger, so when the app hangs
/// before it can draw anything this file is the only evidence of how far it
/// got: the last line in it names the step that did not return.
void begin ();

/// \brief Record that a step is about to run.
void step (char const *what);

/// \brief Record a result for the step that just ran.
void note (char const *what, bool ok);

/// \brief Mark start-up as complete and close the file.
void end ();
}
