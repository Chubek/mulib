/* MuLIB browser staging addition: web-storagekey
 * Implemented subset: Exact equality of serialized origin, top-level site and nonce.
 * Remaining: Opaque origin semantics, partitioning and serialization.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_STORAGEKEY_H
#define MULIB_WEB_STORAGEKEY_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span origin,site;uint64_t nonce;} mulib_web_storagekey_t;
static inline int mulib_web_storagekey_equal(mulib_web_storagekey_t a,mulib_web_storagekey_t b){return a.nonce==b.nonce&&muba_eq(a.origin,b.origin)&&muba_eq(a.site,b.site);}
#endif /* MULIB_WEB_STORAGEKEY_H */
