/* MuLIB browser staging addition: browser-profile
 * Implemented subset: Explicit profile ID, private mode and persistence policy.
 * Remaining: Directory management, origin storage, profile locks and privacy.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_PROFILE_H
#define MULIB_BROWSER_PROFILE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id;int private_mode,allow_persistence;} mulib_browser_profile_t;
static inline int mulib_browser_profile_persist(const mulib_browser_profile_t *p){return p&&p->id&&!p->private_mode&&p->allow_persistence;}
#endif /* MULIB_BROWSER_PROFILE_H */
