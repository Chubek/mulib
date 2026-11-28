/* MuLIB browser staging addition: browser-window
 * Implemented subset: Stable WindowProxy identity with swappable current realm/window.
 * Remaining: Cross-origin proxy semantics, closed state and navigation lifecycle.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_WINDOW_H
#define MULIB_BROWSER_WINDOW_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t proxy,context,realm;uintptr_t window;int closed;} mulib_browser_window_t;
static inline int mulib_browser_window_swap(mulib_browser_window_t *w,uint64_t realm,uintptr_t window){if(!w||!w->proxy||w->closed||!realm||!window)return 0;w->realm=realm;w->window=window;return 1;}
#endif /* MULIB_BROWSER_WINDOW_H */
