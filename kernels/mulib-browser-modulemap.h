/* MuLIB browser staging addition: browser-modulemap
 * Implemented subset: Exact URL/type cache with fetching/ready/failed state.
 * Remaining: URL normalization, import maps, module records, dependency cycles and async fetch.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_MODULEMAP_H
#define MULIB_BROWSER_MODULEMAP_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span url;unsigned type,state;uintptr_t module;} mulib_browser_modulemap_entry;
static inline mulib_browser_modulemap_entry *mulib_browser_modulemap_get(mulib_browser_modulemap_entry *v,size_t n,muba_span url,unsigned type){if(v)for(size_t i=0;i<n;i++)if(v[i].type==type&&muba_eq(v[i].url,url))return v+i;return NULL;}
static inline int mulib_browser_modulemap_add(mulib_browser_modulemap_entry *v,size_t *n,size_t cap,mulib_browser_modulemap_entry e){if(!v||!n||*n>=cap||!e.url.len||!muba_span_ok(e.url)||mulib_browser_modulemap_get(v,*n,e.url,e.type))return 0;v[(*n)++]=e;return 1;}
#endif /* MULIB_BROWSER_MODULEMAP_H */
