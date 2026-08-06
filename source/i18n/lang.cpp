#include "lang.h"

#include <3ds.h>

namespace
{
char const *const s_english[] = {
#define I18N_EN(name, en, es) en,
    I18N_STRINGS (I18N_EN)
#undef I18N_EN
};

char const *const s_spanish[] = {
#define I18N_ES(name, en, es) es,
    I18N_STRINGS (I18N_ES)
#undef I18N_ES
};

static_assert (sizeof (s_english) / sizeof (*s_english) == i18n::STR_COUNT);
static_assert (sizeof (s_spanish) / sizeof (*s_spanish) == i18n::STR_COUNT);

i18n::Language s_language = i18n::ENGLISH;
}

void i18n::init ()
{
	if (R_FAILED (cfguInit ()))
		return;

	u8 language;
	if (R_SUCCEEDED (CFGU_GetSystemLanguage (&language)) && language == CFG_LANGUAGE_ES)
		s_language = SPANISH;

	cfguExit ();
}

void i18n::setLanguage (Language const lang)
{
	s_language = lang;
}

i18n::Language i18n::language ()
{
	return s_language;
}

char const *i18n::tr (Str const str)
{
	if (str < 0 || str >= STR_COUNT)
		return "";

	return s_language == SPANISH ? s_spanish[str] : s_english[str];
}
