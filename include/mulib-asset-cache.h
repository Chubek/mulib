/** @file mulib-asset-cache.h
 * Digest- and variant-keyed non-owning asset cache.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Caller computes SHA-256; no eviction or persistent store.
 */
#ifndef MULIB_ASSET_CACHE_H
#define MULIB_ASSET_CACHE_H
#include "mulib-asset-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint8_t digest[32];uint64_t variant;uintptr_t value;bool occupied;} mulib_asset_cache_entry;
static inline bool mulib_asset_cache_find(const mulib_asset_cache_entry *a,size_t n,const uint8_t digest[32],uint64_t variant,uintptr_t *out){if(!a||!digest||!out)return false;for(size_t i=0;i<n;i++)if(a[i].occupied&&a[i].variant==variant&&!memcmp(a[i].digest,digest,32)){*out=a[i].value;return true;}return false;}
/* Replace the same digest/variant before taking an empty slot. Values are
 * borrowed; replacement does not release the previous value. Caller owns it. */
static inline bool mulib_asset_cache_put(mulib_asset_cache_entry *a,size_t n,mulib_asset_cache_entry v){if(!a)return false;size_t empty=SIZE_MAX;for(size_t i=0;i<n;i++){if(a[i].occupied&&a[i].variant==v.variant&&!memcmp(a[i].digest,v.digest,32)){v.occupied=true;a[i]=v;return true;}if(!a[i].occupied&&empty==SIZE_MAX)empty=i;}if(empty==SIZE_MAX)return false;v.occupied=true;a[empty]=v;return true;}
#ifdef __cplusplus
}
#endif
#endif
