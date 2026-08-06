#include "swkbd.h"

#include "i18n/lang.h"

#include <3ds.h>

#include <vector>

bool ui::swkbd::input (std::string &out,
    char const *const hint,
    std::string const &initial,
    Kind const kind,
    std::size_t const maxLength)
{
	SwkbdState state;
	swkbdInit (&state,
	    kind == NUMERIC ? SWKBD_TYPE_NUMPAD : SWKBD_TYPE_NORMAL,
	    2,
	    static_cast<int> (maxLength));

	swkbdSetHintText (&state, hint);
	swkbdSetInitialText (&state, initial.c_str ());
	swkbdSetButton (&state, SWKBD_BUTTON_LEFT, TR (CANCEL), false);
	swkbdSetButton (&state, SWKBD_BUTTON_RIGHT, TR (OK), true);

	if (kind == PASSWORD)
		swkbdSetPasswordMode (&state, SWKBD_PASSWORD_HIDE_DELAY);

	// Empty input is how a field gets cleared, so accept anything.
	swkbdSetValidation (&state, SWKBD_ANYTHING, 0, 0);

	// UTF-8 can take up to 4 bytes per code point, plus the terminator.
	std::vector<char> buffer (maxLength * 4 + 1, '\0');

	if (swkbdInputText (&state, buffer.data (), buffer.size ()) != SWKBD_BUTTON_RIGHT)
		return false;

	out = buffer.data ();
	return true;
}
