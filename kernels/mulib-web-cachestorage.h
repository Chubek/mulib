/* MuLIB browser staging addition: web-cachestorage
 * Implemented subset: Exact GET method/URL cache matching over caller-supplied records.
 * Remaining: Insertion, Vary, ignore options, storage partitioning, response cloning and persistence.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_CACHESTORAGE_H
#define MULIB_WEB_CACHESTORAGE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span url;uintptr_t response;} mulib_web_cachestorage_entry;
static inline uintptr_t mulib_web_cachestorage_match(const mulib_web_cachestorage_entry *v,size_t n,muba_span method,muba_span url){if(!muba_eq(method,muba_str("GET")))return 0;if(v)for(size_t i=0;i<n;i++)if(muba_eq(v[i].url,url))return v[i].response;return 0;}
#endif /* MULIB_WEB_CACHESTORAGE_H */
