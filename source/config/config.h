#pragma once

#include <cstdint>
#include <string>

namespace config
{
/// Fixed number of saved connections, per the spec.
constexpr int SLOT_COUNT = 5;

struct Slot
{
	std::string host;
	std::uint16_t port = 21;
	std::string user;
	std::string password;
	bool anonymous = false;
	std::string alias;

	bool empty () const
	{
		return host.empty ();
	}

	/// \brief What to show in the slot list: the alias if set, else host:port.
	std::string label () const;

	/// \brief Effective credentials, resolving the anonymous checkbox.
	std::string effectiveUser () const;
	std::string effectivePassword () const;
};

/// \brief Load the config from the SD card. Missing or malformed files leave
/// every slot empty rather than failing.
void load ();

/// \brief Write the config back to the SD card.
bool save ();

Slot &slot (int index);

/// \brief Index of the last slot connected to, or -1 if there is none.
int lastUsed ();
void setLastUsed (int index);

/// \brief Directory the app owns on the SD card, with a trailing slash.
char const *directory ();
}
