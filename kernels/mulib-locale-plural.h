/* MuLIB browser staging addition: locale-plural
 * Implemented subset: English and French integer cardinal one/other selection.
 * Remaining: CLDR operands, fractions, ordinals and other locales.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_LOCALE_PLURAL_H
#define MULIB_LOCALE_PLURAL_H
#include "detail/mulib-browser-additions-common.h"

static inline const char *mulib_locale_plural_cardinal(muba_span lang,int64_t n){if(muba_ieq(lang,muba_str("en")))return n==1||n==-1?"one":"other";if(muba_ieq(lang,muba_str("fr")))return n>=-1&&n<=1?"one":"other";return NULL;}
#endif /* MULIB_LOCALE_PLURAL_H */
