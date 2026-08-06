#pragma once

#include "entry.h"

namespace fs
{
namespace local
{
/// Root of the SD card as libctru's devoptab exposes it.
constexpr char const *ROOT = "sdmc:/";

/// \brief List a directory, sorted directories-first then by name.
/// Contains real entries only; the ".." row is the browser's business.
bool list (std::string const &path, Listing &out);

bool exists (std::string const &path);
bool isDirectory (std::string const &path);
bool size (std::string const &path, std::uint64_t &out);

bool makeDirectory (std::string const &path);
bool removeFile (std::string const &path);
bool removeDirectory (std::string const &path);
bool rename (std::string const &from, std::string const &to);

/// \brief Create every missing component of \a path.
bool makeDirectories (std::string const &path);
}
}
