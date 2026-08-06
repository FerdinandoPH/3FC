#include "ops.h"

#include "ftp/ftpClient.h"
#include "localFs.h"
#include "log.h"

namespace
{
/// Deleting is destructive, so the depth cap here is about not walking off the
/// end of the worker's stack on a tree that reports itself as its own child.
constexpr int MAX_DEPTH = 32;
}

bool fs::ops::isDirectory (ftp::Client &client, Machine const machine, std::string const &path)
{
	if (machine == LOCAL)
		return local::isDirectory (path);

	std::uint64_t size;
	return client.probe (path, size) == ftp::Client::PROBE_DIRECTORY;
}

bool fs::ops::removeTree (ftp::Client &client,
    Machine const machine,
    std::string const &path,
    int const depth)
{
	if (depth > MAX_DEPTH)
	{
		logger::printf (logger::ERROR, "Too many nested folders under %s", path.c_str ());
		return false;
	}

	bool directory;

	if (machine == LOCAL)
		directory = local::isDirectory (path);
	else
	{
		// Never guess from a failed query. Deleting is the one operation where
		// acting on a wrong answer cannot be undone, so a server we could not
		// ask is a reason to stop rather than to assume.
		std::uint64_t size;
		switch (client.probe (path, size))
		{
		case ftp::Client::PROBE_FILE:
			directory = false;
			break;

		case ftp::Client::PROBE_DIRECTORY:
			directory = true;
			break;

		default:
			logger::printf (logger::ERROR, "Could not inspect %s", path.c_str ());
			return false;
		}
	}

	if (!directory)
	{
		auto const ok = machine == LOCAL ? local::removeFile (path) : client.removeFile (path);
		if (!ok)
			logger::printf (logger::ERROR, "Could not delete %s", path.c_str ());

		return ok;
	}

	Listing listing;
	auto const listed = machine == LOCAL ? local::list (path, listing) : client.list (path, listing);
	if (!listed)
		return false;

	// Depth first: a directory can only be removed once it is empty.
	for (auto const &entry : listing)
	{
		if (entry.type == PARENT || entry.name == "." || entry.name == "..")
			continue;

		if (!removeTree (client, machine, join (path, entry.name), depth + 1))
			return false;
	}

	auto const ok = machine == LOCAL ? local::removeDirectory (path) : client.removeDirectory (path);
	if (!ok)
		logger::printf (logger::ERROR, "Could not delete %s", path.c_str ());

	return ok;
}

bool fs::ops::makeDirectory (ftp::Client &client, Machine const machine, std::string const &path)
{
	auto const ok = machine == LOCAL ? local::makeDirectory (path) : client.makeDirectory (path);
	if (!ok)
		logger::printf (logger::ERROR, "Could not create %s", path.c_str ());

	return ok;
}

bool fs::ops::rename (ftp::Client &client,
    Machine const machine,
    std::string const &from,
    std::string const &to)
{
	auto const ok = machine == LOCAL ? local::rename (from, to) : client.rename (from, to);
	if (!ok)
		logger::printf (logger::ERROR, "Could not rename %s", from.c_str ());

	return ok;
}
