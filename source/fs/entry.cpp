#include "entry.h"

#include <cstdio>

std::string fs::join (std::string const &dir, std::string const &name)
{
	if (dir.empty ())
		return name;

	if (dir.back () == '/')
		return dir + name;

	return dir + "/" + name;
}

bool fs::isRoot (std::string const &path)
{
	if (path.empty () || path == "/")
		return true;

	// "sdmc:/", and "sdmc:" for good measure even though we never produce it.
	auto const colon = path.find (':');
	if (colon == std::string::npos)
		return false;

	return colon + 1 == path.size () || (colon + 2 == path.size () && path.back () == '/');
}

std::string fs::parent (std::string const &path)
{
	if (isRoot (path))
		return path;

	// A trailing separator is not a component: parent("sdmc:/a/b/") is "sdmc:/a".
	auto end = path.size ();
	if (path[end - 1] == '/')
		--end;

	auto const slash = path.find_last_of ('/', end - 1);
	if (slash == std::string::npos)
		return "/";

	auto result = path.substr (0, slash);

	// A bare device name is not a usable path: libctru resolves "sdmc:" to that
	// device's *working* directory, which after launching from the Homebrew
	// Launcher is the app's own folder rather than the card's root.
	if (result.empty () || result.back () == ':')
		result += '/';

	return result;
}

std::string fs::basename (std::string const &path)
{
	auto const slash = path.find_last_of ('/');
	if (slash == std::string::npos)
		return path;

	return path.substr (slash + 1);
}

std::string fs::formatSize (std::uint64_t const bytes)
{
	static char const *const units[] = {"B", "KB", "MB", "GB", "TB"};

	auto value  = static_cast<double> (bytes);
	int unit    = 0;
	while (value >= 1024.0 && unit + 1 < 5)
	{
		value /= 1024.0;
		++unit;
	}

	char buffer[32];
	if (unit == 0)
		std::snprintf (buffer, sizeof (buffer), "%llu B", static_cast<unsigned long long> (bytes));
	else
		std::snprintf (buffer, sizeof (buffer), "%.1f %s", value, units[unit]);

	return buffer;
}
