#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fs
{
enum Type
{
	FILE,
	DIRECTORY,
	PARENT, ///< the synthetic ".." row
};

struct Entry
{
	std::string name;
	Type type          = FILE;
	std::uint64_t size = 0;

	bool isDir () const
	{
		return type != FILE;
	}
};

using Listing = std::vector<Entry>;

/// \brief Which side of the transfer an entry or selection belongs to.
enum Machine
{
	LOCAL,  ///< the console's SD card
	REMOTE, ///< the FTP server
};

/// \brief Join a directory and a name into a path, collapsing the separator.
std::string join (std::string const &dir, std::string const &name);

/// \brief Whether \a path is the top of its tree and so has no parent.
/// Covers both the server's "/" and the SD card's "sdmc:/".
bool isRoot (std::string const &path);

/// \brief Everything up to the last separator; the root itself for a top-level
/// path. Never returns a bare device name such as "sdmc:".
std::string parent (std::string const &path);

/// \brief Everything after the last separator.
std::string basename (std::string const &path);

/// \brief Human-readable byte count ("1.4 MB").
std::string formatSize (std::uint64_t bytes);
}
