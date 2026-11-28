/* MuLIB browser staging addition: fetch-preflight-cache
 * Implemented subset: Expiry and exact origin/URL/method/credential key lookup.
 * Remaining: Partition keys, header names, wildcard, max age and eviction.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_PREFLIGHT_CACHE_H
#define MULIB_FETCH_PREFLIGHT_CACHE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span origin,url,method;int credentials;uint64_t expires;} mulib_fetch_preflight_cache_entry;
static inline int mulib_fetch_preflight_cache_hit(const mulib_fetch_preflight_cache_entry *v,size_t n,mulib_fetch_preflight_cache_entry key,uint64_t now){if(v)for(size_t i=0;i<n;i++)if(v[i].expires>now&&v[i].credentials==key.credentials&&muba_eq(v[i].origin,key.origin)&&muba_eq(v[i].url,key.url)&&muba_eq(v[i].method,key.method))return 1;return 0;}
#endif /* MULIB_FETCH_PREFLIGHT_CACHE_H */
