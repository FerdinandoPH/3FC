#include "ftpListing.h"

#include <cstdlib>
#include <cstring>

namespace
{
bool equalsIgnoreCase (std::string const &a, char const *const b)
{
	return ::strcasecmp (a.c_str (), b) == 0;
}
}

bool ftp::parseMlsdLine (std::string const &line, fs::Entry &out)
{
	// RFC 3659: "fact=value;fact=value; filename"
	auto const space = line.find (' ');
	if (space == std::string::npos || space + 1 >= line.size ())
		return false;

	auto const name = line.substr (space + 1);
	if (name.empty () || name == "." || name == "..")
		return false;

	std::string type;
	std::uint64_t size = 0;

	std::size_t pos = 0;
	while (pos < space)
	{
		auto end = line.find (';', pos);
		if (end == std::string::npos || end > space)
			end = space;

		auto const fact = line.substr (pos, end - pos);
		pos             = end + 1;

		auto const eq = fact.find ('=');
		if (eq == std::string::npos)
			continue;

		auto const key   = fact.substr (0, eq);
		auto const value = fact.substr (eq + 1);

		if (equalsIgnoreCase (key, "type"))
			type = value;
		else if (equalsIgnoreCase (key, "size"))
			size = std::strtoull (value.c_str (), nullptr, 10);
	}

	// cdir/pdir describe the listed directory itself and its parent; the ".."
	// row is synthesised by the caller instead.
	if (equalsIgnoreCase (type, "cdir") || equalsIgnoreCase (type, "pdir"))
		return false;

	if (type.empty ())
		return false;

	out.name = name;
	out.type = equalsIgnoreCase (type, "dir") ? fs::DIRECTORY : fs::FILE;
	out.size = out.type == fs::DIRECTORY ? 0 : size;

	return true;
}

bool ftp::parseListLine (std::string const &line, fs::Entry &out)
{
	// "drwxr-xr-x  2 owner group  4096 Jan  1 00:00 name with spaces"
	// Fields are whitespace-separated up to the name, which starts after the
	// 8th field and may itself contain spaces.
	if (line.size () < 10)
		return false;

	auto const directory = line[0] == 'd';
	auto const link      = line[0] == 'l';

	std::size_t pos   = 0;
	std::uint64_t size = 0;

	for (int field = 0; field < 8; ++field)
	{
		while (pos < line.size () && line[pos] == ' ')
			++pos;

		auto const start = pos;
		while (pos < line.size () && line[pos] != ' ')
			++pos;

		if (pos >= line.size ())
			return false;

		// field 4 (0-based) is the size
		if (field == 4)
			size = std::strtoull (line.substr (start, pos - start).c_str (), nullptr, 10);
	}

	while (pos < line.size () && line[pos] == ' ')
		++pos;

	if (pos >= line.size ())
		return false;

	auto name = line.substr (pos);

	// Symlinks are listed as "name -> target"; keep the name and treat the
	// link as a file, since following it reliably needs a stat we cannot do.
	if (link)
	{
		auto const arrow = name.find (" -> ");
		if (arrow != std::string::npos)
			name = name.substr (0, arrow);
	}

	if (name.empty () || name == "." || name == "..")
		return false;

	out.name = std::move (name);
	out.type = directory ? fs::DIRECTORY : fs::FILE;
	out.size = directory ? 0 : size;

	return true;
}
