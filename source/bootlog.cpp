#include "bootlog.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>

namespace
{
std::FILE *s_file = nullptr;

void flush ()
{
	if (!s_file)
		return;

	std::fflush (s_file);
	// fflush only reaches newlib's layer; fsync pushes it to the SD so the line
	// survives a hard power-off, which is the only way out of a hang.
	::fsync (::fileno (s_file));
}
}

void bootlog::begin ()
{
	::mkdir ("sdmc:/3ds", 0777);
	::mkdir ("sdmc:/3ds/3FC", 0777);

	s_file = std::fopen ("sdmc:/3ds/3FC/boot.log", "w");
	if (!s_file)
		return;

	std::fprintf (s_file, "3FC boot log\n");
	flush ();
}

void bootlog::step (char const *const what)
{
	if (!s_file)
		return;

	std::fprintf (s_file, "-> %s\n", what);
	flush ();
}

void bootlog::note (char const *const what, bool const ok)
{
	if (!s_file)
		return;

	std::fprintf (s_file, "   %s: %s\n", what, ok ? "ok" : "FAILED");
	flush ();
}

void bootlog::end ()
{
	if (!s_file)
		return;

	std::fprintf (s_file, "startup complete\n");
	flush ();

	std::fclose (s_file);
	s_file = nullptr;
}
