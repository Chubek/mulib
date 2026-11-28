/* MuLIB browser staging addition: web-corp
 * Implemented subset: Exact recognized policy token to numeric ID; unknown returns -1.
 * Remaining: Actual request mode and response policy enforcement.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_CORP_H
#define MULIB_WEB_CORP_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_corp_parse(muba_span s){const char *v[]={ "same-origin","same-site","cross-origin" };s=muba_trim(s);for(size_t i=0;i<sizeof v/sizeof *v;i++)if(muba_eq(s,muba_str(v[i])))return (int)i;return -1;}
#endif /* MULIB_WEB_CORP_H */
