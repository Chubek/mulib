/* MuLIB browser staging addition: browser-frameclock
 * Implemented subset: Caller-clocked frame tick with nondecreasing timestamp and sequence.
 * Remaining: Refresh scheduling, requestAnimationFrame and lifecycle integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_FRAMECLOCK_H
#define MULIB_BROWSER_FRAMECLOCK_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t frame,time;} mulib_browser_frameclock_t;
static inline int mulib_browser_frameclock_tick(mulib_browser_frameclock_t *c,uint64_t time){if(!c||time<c->time||c->frame==UINT64_MAX)return 0;c->time=time;c->frame++;return 1;}
#endif /* MULIB_BROWSER_FRAMECLOCK_H */
