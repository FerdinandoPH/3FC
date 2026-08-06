#pragma once

#include <cstddef>
#include <string>

namespace ui
{
namespace swkbd
{
enum Kind
{
	TEXT,
	NUMERIC,
	PASSWORD,
};

/// \brief Open the console's native keyboard.
/// \param out Receives the entered text; untouched if the user cancels
/// \param hint Shown as the keyboard's hint text
/// \param initial Text the keyboard opens with
/// \param maxLength Maximum number of bytes (UTF-8) to accept
/// \returns false if the user cancelled
bool input (std::string &out,
    char const *hint,
    std::string const &initial,
    Kind kind         = TEXT,
    std::size_t maxLength = 255);
}
}
