#pragma once

#include "entry.h"

namespace ftp
{
class Client;
}

namespace fs
{
namespace ops
{
/// Every function here runs on the network worker: the remote variants issue
/// FTP commands, and the local ones can walk a large tree. \a client is ignored
/// for LOCAL paths.

/// \brief Delete a file, or a directory and everything under it.
///
/// Stops at the first thing it cannot delete *or cannot identify*: a server
/// that stopped answering is never taken to mean "empty directory".
///
/// \param depth Recursion guard; callers pass 0
bool removeTree (ftp::Client &client, Machine machine, std::string const &path, int depth = 0);

bool makeDirectory (ftp::Client &client, Machine machine, std::string const &path);

bool rename (ftp::Client &client,
    Machine machine,
    std::string const &from,
    std::string const &to);

/// \brief Whether \a path names an existing directory.
bool isDirectory (ftp::Client &client, Machine machine, std::string const &path);
}
}
