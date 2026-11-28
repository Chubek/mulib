/* MuLIB browser staging addition: web-sharedworker
 * Implemented subset: Match shared worker identity by origin key, URL and name.
 * Remaining: Connection ports, lifecycle, policy and process integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_SHAREDWORKER_H
#define MULIB_WEB_SHAREDWORKER_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-web-storagekey.h"
static inline int mulib_web_sharedworker_match(mulib_web_storagekey_t a,muba_span au,muba_span an,mulib_web_storagekey_t b,muba_span bu,muba_span bn){return mulib_web_storagekey_equal(a,b)&&muba_eq(au,bu)&&muba_eq(an,bn);}
#endif /* MULIB_WEB_SHAREDWORKER_H */
