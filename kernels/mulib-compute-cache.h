/** @file mulib-compute-cache.h
 * Device/revision-keyed bounded pipeline handle cache.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No eviction or handle ownership.
 */
#ifndef MULIB_COMPUTE_CACHE_H
#define MULIB_COMPUTE_CACHE_H
#include "mulib-compute-core.h"
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
typedef struct {uint64_t key,device,revision,pipeline;bool used;} mulib_compute_cache_entry;
static inline bool mulib_compute_cache_find(const mulib_compute_cache_entry *a,size_t n,uint64_t key,uint64_t device,uint64_t revision,uint64_t *pipeline){if(!a||!pipeline||n>SIZE_MAX/sizeof *a)return false;for(size_t i=0;i<n;i++)if(a[i].used&&a[i].key==key&&a[i].device==device&&a[i].revision==revision){*pipeline=a[i].pipeline;return true;}return false;}
/* Upsert an exact key/device/revision. Caller owns and releases any replaced
 * pipeline; this cache never frees native handles. a has n initialized entries. */
static inline bool mulib_compute_cache_add(mulib_compute_cache_entry *a,size_t n,mulib_compute_cache_entry e){
 if(!a||!e.pipeline||n>SIZE_MAX/sizeof *a)return false;
 size_t free_slot=SIZE_MAX;e.used=true;
 for(size_t i=0;i<n;i++){
  if(a[i].used&&a[i].key==e.key&&a[i].device==e.device&&a[i].revision==e.revision){a[i]=e;return true;}
  if(!a[i].used&&free_slot==SIZE_MAX)free_slot=i;
 }
 if(free_slot==SIZE_MAX)return false;
 a[free_slot]=e;return true;
}
#ifdef __cplusplus
}
#endif
#endif
