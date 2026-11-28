/* MuLIB browser staging addition: browser-activation
 * Implemented subset: Transient/sticky activation timestamp, expiry and consumption.
 * Remaining: Trusted input integration, propagation and HTML activation behavior.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_ACTIVATION_H
#define MULIB_BROWSER_ACTIVATION_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t time;int sticky,consumed;} mulib_browser_activation_t;
static inline void mulib_browser_activation_activate(mulib_browser_activation_t *a,uint64_t now){if(a)*a=(mulib_browser_activation_t){now,1,0};}
static inline int mulib_browser_activation_active(const mulib_browser_activation_t *a,uint64_t now,uint64_t ttl){return a&&a->sticky&&!a->consumed&&now>=a->time&&now-a->time<ttl;}
static inline int mulib_browser_activation_consume(mulib_browser_activation_t *a,uint64_t now,uint64_t ttl){if(!mulib_browser_activation_active(a,now,ttl))return 0;a->consumed=1;return 1;}
#endif /* MULIB_BROWSER_ACTIVATION_H */
