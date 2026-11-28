/* MuLIB browser staging addition: browser-lifecycle
 * Implemented subset: Monotonic loading/interactive/complete document readiness transition.
 * Remaining: DOMContentLoaded/load sequencing, load blockers and unload semantics.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_LIFECYCLE_H
#define MULIB_BROWSER_LIFECYCLE_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_browser_lifecycle_LOADING,mulib_browser_lifecycle_INTERACTIVE,mulib_browser_lifecycle_COMPLETE} mulib_browser_lifecycle_state;
static inline int mulib_browser_lifecycle_advance(mulib_browser_lifecycle_state *s,mulib_browser_lifecycle_state next){if(!s||*s<mulib_browser_lifecycle_LOADING||*s>=mulib_browser_lifecycle_COMPLETE||next!=*s+1)return 0;*s=next;return 1;}
#endif /* MULIB_BROWSER_LIFECYCLE_H */
