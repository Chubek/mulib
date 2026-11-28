/* MuLIB browser staging addition: webidl-wrappercache
 * Implemented subset: Bounded per-realm native-to-wrapper cache with explicit removal.
 * Remaining: Weak rooting, GC cleanup and object lifetime.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_WRAPPERCACHE_H
#define MULIB_WEBIDL_WRAPPERCACHE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t realm;uintptr_t native,wrapper;} mulib_webidl_wrappercache_entry;
static inline uintptr_t mulib_webidl_wrappercache_get(const mulib_webidl_wrappercache_entry *v,size_t n,uint64_t realm,uintptr_t native){if(v)for(size_t i=0;i<n;i++)if(v[i].realm==realm&&v[i].native==native)return v[i].wrapper;return 0;}
static inline int mulib_webidl_wrappercache_put(mulib_webidl_wrappercache_entry *v,size_t *n,size_t cap,mulib_webidl_wrappercache_entry e){if(!v||!n||*n>=cap||!e.realm||!e.native||!e.wrapper||mulib_webidl_wrappercache_get(v,*n,e.realm,e.native))return 0;v[(*n)++]=e;return 1;}
static inline int mulib_webidl_wrappercache_remove(mulib_webidl_wrappercache_entry *v,size_t *n,uint64_t realm,uintptr_t native){if(!v||!n)return 0;for(size_t i=0;i<*n;i++)if(v[i].realm==realm&&v[i].native==native){v[i]=v[--*n];return 1;}return 0;}
#endif /* MULIB_WEBIDL_WRAPPERCACHE_H */
