#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace logger
{
enum Level
{
	SENT,     ///< command we sent to the server
	RECEIVED, ///< response line from the server
	INFO,
	ERROR,
	DEBUG,
};

struct Line
{
	Level level;
	std::string text;
};

/// The console panel is written from the network worker and read from the UI
/// thread, so every entry point here takes the lock.
void init ();
void exit ();

void add (Level level, std::string text);
void printf (Level level, char const *format, ...) __attribute__ ((format (printf, 2, 3)));

/// \brief Copy the buffer out for rendering.
void snapshot (std::vector<Line> &out);

void clear ();
}
