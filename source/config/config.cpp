#include "config.h"

#include "log.h"

#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
constexpr char const *DIRECTORY = "sdmc:/3ds/3FC/";
constexpr char const *FILENAME  = "sdmc:/3ds/3FC/config.cfg";

config::Slot s_slots[config::SLOT_COUNT];
int s_lastUsed = -1;

void trimEol (std::string &line)
{
	while (!line.empty () && (line.back () == '\n' || line.back () == '\r'))
		line.pop_back ();
}

/// \brief Split "key=value" at the first '='. Values keep spaces and UTF-8 as
/// they are, so aliases and paths survive a round trip.
bool split (std::string const &line, std::string &key, std::string &value)
{
	auto const eq = line.find ('=');
	if (eq == std::string::npos)
		return false;

	key   = line.substr (0, eq);
	value = line.substr (eq + 1);
	return true;
}
}

std::string config::Slot::label () const
{
	if (!alias.empty ())
		return alias;

	if (host.empty ())
		return {};

	return host + ":" + std::to_string (port);
}

std::string config::Slot::effectiveUser () const
{
	return anonymous ? "anonymous" : user;
}

std::string config::Slot::effectivePassword () const
{
	// The conventional anonymous password is an e-mail address; servers that
	// care generally accept anything with an '@'.
	return anonymous ? "3fc@3ds" : password;
}

char const *config::directory ()
{
	return DIRECTORY;
}

void config::load ()
{
	for (auto &slot : s_slots)
		slot = Slot{};
	s_lastUsed = -1;

	auto *const fp = std::fopen (FILENAME, "rb");
	if (!fp)
		return;

	char buffer[512];
	int current = -1;

	while (std::fgets (buffer, sizeof (buffer), fp))
	{
		std::string line = buffer;
		trimEol (line);

		if (line.empty () || line[0] == '#')
			continue;

		if (line[0] == '[')
		{
			current = -1;
			if (line.size () >= 7 && line.compare (0, 5, "[slot") == 0)
			{
				auto const index = line[5] - '0';
				if (index >= 0 && index < SLOT_COUNT)
					current = index;
			}
			continue;
		}

		std::string key, value;
		if (!split (line, key, value))
			continue;

		if (current < 0)
		{
			if (key == "last")
			{
				auto const index = std::atoi (value.c_str ());
				if (index >= 0 && index < SLOT_COUNT)
					s_lastUsed = index;
			}
			continue;
		}

		auto &slot = s_slots[current];

		if (key == "host")
			slot.host = value;
		else if (key == "port")
		{
			auto const port = std::atoi (value.c_str ());
			slot.port       = (port > 0 && port <= 65535) ? port : 21;
		}
		else if (key == "user")
			slot.user = value;
		else if (key == "pass")
			slot.password = value;
		else if (key == "anon")
			slot.anonymous = value == "1";
		else if (key == "alias")
			slot.alias = value;
	}

	std::fclose (fp);

	// A "last used" pointing at a slot that was since cleared is meaningless.
	if (s_lastUsed >= 0 && s_slots[s_lastUsed].empty ())
		s_lastUsed = -1;
}

bool config::save ()
{
	::mkdir ("sdmc:/3ds", 0777);
	::mkdir ("sdmc:/3ds/3FC", 0777);

	auto *const fp = std::fopen (FILENAME, "wb");
	if (!fp)
	{
		logger::add (logger::ERROR, "Could not write config.cfg");
		return false;
	}

	std::fprintf (fp,
	    "# 3FC\n"
	    "# WARNING: passwords are stored in plain text. The SD card is readable\n"
	    "# by anyone with physical access to the console.\n"
	    "# AVISO: las contrasenas se guardan en texto plano. La tarjeta SD es\n"
	    "# legible por cualquiera que tenga acceso fisico a la consola.\n"
	    "last=%d\n",
	    s_lastUsed);

	for (int i = 0; i < SLOT_COUNT; ++i)
	{
		auto const &slot = s_slots[i];
		if (slot.empty ())
			continue;

		std::fprintf (fp,
		    "\n[slot%d]\n"
		    "host=%s\n"
		    "port=%u\n"
		    "user=%s\n"
		    "pass=%s\n"
		    "anon=%d\n"
		    "alias=%s\n",
		    i,
		    slot.host.c_str (),
		    slot.port,
		    slot.user.c_str (),
		    slot.password.c_str (),
		    slot.anonymous ? 1 : 0,
		    slot.alias.c_str ());
	}

	std::fclose (fp);
	return true;
}

config::Slot &config::slot (int const index)
{
	static Slot dummy;

	if (index < 0 || index >= SLOT_COUNT)
		return dummy;

	return s_slots[index];
}

int config::lastUsed ()
{
	return s_lastUsed;
}

void config::setLastUsed (int const index)
{
	if (index >= 0 && index < SLOT_COUNT)
		s_lastUsed = index;
}
