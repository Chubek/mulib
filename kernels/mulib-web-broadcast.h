/* MuLIB browser staging addition: web-broadcast
 * Implemented subset: Channel and storage key match predicate for routing.
 * Remaining: Registration, serialization, asynchronous delivery and partition policy.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_BROADCAST_H
#define MULIB_WEB_BROADCAST_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-web-storagekey.h"
static inline int mulib_web_broadcast_matches(mulib_web_storagekey_t a,muba_span ac,mulib_web_storagekey_t b,muba_span bc){return mulib_web_storagekey_equal(a,b)&&muba_eq(ac,bc);}
#endif /* MULIB_WEB_BROADCAST_H */
