#pragma once

#include "fs/entry.h"

#include <string>

namespace ftp
{
/// \brief Parse one MLSD line ("type=file;size=12; name").
/// \returns false for lines to skip (cdir/pdir entries, malformed input)
bool parseMlsdLine (std::string const &line, fs::Entry &out);

/// \brief Parse one UNIX-style LIST line, the fallback for servers without
/// MLSD. Names containing spaces are preserved.
bool parseListLine (std::string const &line, fs::Entry &out);
}
