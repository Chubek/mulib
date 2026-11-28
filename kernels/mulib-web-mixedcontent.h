/* MuLIB browser staging addition: web-mixedcontent
 * Implemented subset: Recognize three host policy labels; not a standard HTTP header parser.
 * Remaining: Actual mixed-content categorization and upgrade policy.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_MIXEDCONTENT_H
#define MULIB_WEB_MIXEDCONTENT_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_mixedcontent_parse(muba_span s){const char *v[]={ "block","upgrade","allow" };s=muba_trim(s);for(size_t i=0;i<sizeof v/sizeof *v;i++)if(muba_eq(s,muba_str(v[i])))return (int)i;return -1;}
#endif /* MULIB_WEB_MIXEDCONTENT_H */
