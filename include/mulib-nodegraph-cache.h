/** @file mulib-nodegraph-cache.h
 * Revision-keyed non-owning intermediate result cache.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No eviction; caller releases values.
 */
#ifndef MULIB_NODEGRAPH_CACHE_H
#define MULIB_NODEGRAPH_CACHE_H
#include "mulib-nodegraph-core.h"
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
typedef struct {uint64_t node,revision,key;uintptr_t value;bool occupied;} mulib_nodegraph_cache_entry;
static inline bool mulib_nodegraph_cache_lookup(const mulib_nodegraph_cache_entry *a,size_t n,uint64_t node,uint64_t rev,uint64_t key,uintptr_t *out){size_t bytes;if(!out||!mulib_nodegraph_span(a,n,sizeof *a,&bytes))return false;for(size_t i=0;i<n;i++)if(a[i].occupied&&a[i].node==node&&a[i].revision==rev&&a[i].key==key){*out=a[i].value;return true;}return false;}
/* Duplicate keys are rejected, not silently shadowed. The caller retains
 * ownership of both values and must explicitly remove/replace old entries. */
static inline bool mulib_nodegraph_cache_insert(mulib_nodegraph_cache_entry *a,size_t n,mulib_nodegraph_cache_entry v){size_t bytes;if(!mulib_nodegraph_span(a,n,sizeof *a,&bytes))return false;size_t free_slot=n;for(size_t i=0;i<n;i++){if(a[i].occupied){if(a[i].node==v.node&&a[i].revision==v.revision&&a[i].key==v.key)return false;}else if(free_slot==n)free_slot=i;}if(free_slot==n)return false;v.occupied=true;a[free_slot]=v;return true;}
#ifdef __cplusplus
}
#endif
#endif
