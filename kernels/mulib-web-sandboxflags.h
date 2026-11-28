/* MuLIB browser staging addition: web-sandboxflags
 * Implemented subset: Conservative union of inherited restriction masks.
 * Remaining: HTML sandbox token parsing, navigation and enforcement.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_SANDBOXFLAGS_H
#define MULIB_WEB_SANDBOXFLAGS_H
#include "detail/mulib-browser-additions-common.h"

static inline uint64_t mulib_web_sandboxflags_inherit(uint64_t ancestor,uint64_t frame){return ancestor|frame;}
static inline int mulib_web_sandboxflags_restricted(uint64_t flags,unsigned action){return action>=64||((flags>>action)&1u);}
#endif /* MULIB_WEB_SANDBOXFLAGS_H */
