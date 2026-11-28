/* MuLIB browser staging addition: web-coop
 * Implemented subset: Exact recognized policy token to numeric ID; unknown returns -1.
 * Remaining: Browsing context group switching and opener isolation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_COOP_H
#define MULIB_WEB_COOP_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_coop_parse(muba_span s){const char *v[]={ "unsafe-none","same-origin-allow-popups","same-origin","noopener-allow-popups" };s=muba_trim(s);for(size_t i=0;i<sizeof v/sizeof *v;i++)if(muba_eq(s,muba_str(v[i])))return (int)i;return -1;}
#endif /* MULIB_WEB_COOP_H */
