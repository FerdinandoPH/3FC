#include "localFs.h"

#include "log.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

bool fs::local::list (std::string const &path, Listing &out)
{
	out.clear ();

	auto *const dir = ::opendir (path.c_str ());
	if (!dir)
	{
		logger::printf (logger::ERROR, "opendir %s: %s", path.c_str (), std::strerror (errno));
		return false;
	}

	while (auto const *const entry = ::readdir (dir))
	{
		if (std::strcmp (entry->d_name, ".") == 0 || std::strcmp (entry->d_name, "..") == 0)
			continue;

		Entry item;
		item.name = entry->d_name;

		// d_type is unreliable on some devoptabs, so stat() decides.
		struct stat st;
		if (::stat (join (path, item.name).c_str (), &st) == 0)
		{
			item.type = S_ISDIR (st.st_mode) ? DIRECTORY : FILE;
			item.size = S_ISDIR (st.st_mode) ? 0 : static_cast<std::uint64_t> (st.st_size);
		}
		else
			item.type = (entry->d_type == DT_DIR) ? DIRECTORY : FILE;

		out.push_back (std::move (item));
	}

	::closedir (dir);

	std::sort (out.begin (), out.end (), [] (Entry const &a, Entry const &b) {
		if (a.isDir () != b.isDir ())
			return a.isDir ();

		return ::strcasecmp (a.name.c_str (), b.name.c_str ()) < 0;
	});

	return true;
}

bool fs::local::exists (std::string const &path)
{
	struct stat st;
	return ::stat (path.c_str (), &st) == 0;
}

bool fs::local::isDirectory (std::string const &path)
{
	struct stat st;
	return ::stat (path.c_str (), &st) == 0 && S_ISDIR (st.st_mode);
}

bool fs::local::size (std::string const &path, std::uint64_t &out)
{
	struct stat st;
	if (::stat (path.c_str (), &st) != 0)
		return false;

	out = static_cast<std::uint64_t> (st.st_size);
	return true;
}

bool fs::local::makeDirectory (std::string const &path)
{
	return ::mkdir (path.c_str (), 0777) == 0 || errno == EEXIST;
}

bool fs::local::removeFile (std::string const &path)
{
	return ::unlink (path.c_str ()) == 0;
}

bool fs::local::removeDirectory (std::string const &path)
{
	return ::rmdir (path.c_str ()) == 0;
}

bool fs::local::rename (std::string const &from, std::string const &to)
{
	return ::rename (from.c_str (), to.c_str ()) == 0;
}

bool fs::local::makeDirectories (std::string const &path)
{
	// Skip past the "sdmc:/" prefix: it is not a component that can be created.
	auto start = path.find ('/');
	if (start == std::string::npos)
		return false;

	while (true)
	{
		auto const slash = path.find ('/', start + 1);
		if (slash == std::string::npos)
			break;

		auto const component = path.substr (0, slash);
		if (!component.empty () && !fs::isRoot (component) && !makeDirectory (component))
			return false;

		start = slash;
	}

	return makeDirectory (path);
}
