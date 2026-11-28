/* MuLIB browser staging addition: browser-environment
 * Implemented subset: Environment settings metadata and conservative secure-exposure predicate.
 * Remaining: Incumbent/current/relevant settings, origin/URL and execution integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_ENVIRONMENT_H
#define MULIB_BROWSER_ENVIRONMENT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t realm,origin,context,agent;int secure,active;} mulib_browser_environment_t;
static inline int mulib_browser_environment_allows(const mulib_browser_environment_t *e,int secure_required){return e&&e->realm&&e->active&&(!secure_required||e->secure);}
#endif /* MULIB_BROWSER_ENVIRONMENT_H */
