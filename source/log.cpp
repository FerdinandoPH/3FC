#include "log.h"

#include <3ds.h>

#include <cstdarg>
#include <cstdio>
#include <deque>

namespace
{
/// Bounded so a long session cannot exhaust the 3DS heap.
constexpr std::size_t MAX_LINES = 512;

LightLock s_lock;
std::deque<logger::Line> s_lines;

struct Guard
{
	Guard ()
	{
		LightLock_Lock (&s_lock);
	}
	~Guard ()
	{
		LightLock_Unlock (&s_lock);
	}
};
}

void logger::init ()
{
	LightLock_Init (&s_lock);
}

void logger::exit ()
{
	Guard guard;
	s_lines.clear ();
}

void logger::add (Level const level, std::string text)
{
	// strip the CRLF the FTP protocol puts on every line
	while (!text.empty () && (text.back () == '\r' || text.back () == '\n'))
		text.pop_back ();

	Guard guard;

	s_lines.push_back (Line{level, std::move (text)});
	while (s_lines.size () > MAX_LINES)
		s_lines.pop_front ();
}

void logger::printf (Level const level, char const *const format, ...)
{
	char buffer[512];

	std::va_list ap;
	va_start (ap, format);
	std::vsnprintf (buffer, sizeof (buffer), format, ap);
	va_end (ap);

	add (level, buffer);
}

void logger::snapshot (std::vector<Line> &out)
{
	Guard guard;

	out.assign (s_lines.begin (), s_lines.end ());
}

void logger::clear ()
{
	Guard guard;
	s_lines.clear ();
}
