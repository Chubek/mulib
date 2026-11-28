/* MuLIB browser staging addition: web-beacon
 * Implemented subset: Bounded keepalive payload reservation and release.
 * Remaining: Fetch scheduling, origin policy and asynchronous send.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_BEACON_H
#define MULIB_WEB_BEACON_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_beacon_reserve(size_t *used,size_t cap,size_t n){if(!used||*used>cap||n>cap-*used)return 0;*used+=n;return 1;}
static inline int mulib_web_beacon_release(size_t *used,size_t n){if(!used||n>*used)return 0;*used-=n;return 1;}
#endif /* MULIB_WEB_BEACON_H */
