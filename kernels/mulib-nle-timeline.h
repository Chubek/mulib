/** @file mulib-nle-timeline.h
 * Caller-backed timeline with exact integer ticks and unique clip IDs.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No automatic overlap policy; source and sequence use same tick units.
 */
#ifndef MULIB_NLE_TIMELINE_H
#define MULIB_NLE_TIMELINE_H
#include "mulib-media-core.h"
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
typedef struct {uint64_t id,asset,track;int64_t start,length,source; } mulib_nle_item;
typedef struct {mulib_nle_item *items;size_t count,capacity;mulib_media_rational timebase;} mulib_nle_timeline;
static inline bool mulib_nle_item_valid(mulib_nle_item c){int64_t e;return c.id&&c.asset&&c.track&&c.start>=0&&c.source>=0&&c.length>0&&mulib_media_i64_add(c.start,c.length,&e)&&mulib_media_i64_add(c.source,c.length,&e);}
/* items borrows capacity elements. Validation checks all live items and ID
 * uniqueness (quadratic); callers bound count to their application work budget.
 * Track overlap/locking remains caller policy. */
static inline bool mulib_nle_timeline_valid(const mulib_nle_timeline *t){
 if(!t||t->count>t->capacity||t->capacity>SIZE_MAX/sizeof *t->items||
    (t->capacity&&!t->items)||t->timebase.num<=0||t->timebase.den<=0)return false;
 for(size_t i=0;i<t->count;i++){
  if(!mulib_nle_item_valid(t->items[i]))return false;
  for(size_t j=0;j<i;j++)if(t->items[j].id==t->items[i].id)return false;
 }
 return true;
}
static inline size_t mulib_nle_timeline_find(const mulib_nle_timeline *t,uint64_t id){if(mulib_nle_timeline_valid(t))for(size_t i=0;i<t->count;i++)if(t->items[i].id==id)return i;return SIZE_MAX;}
static inline bool mulib_nle_timeline_insert(mulib_nle_timeline *t,mulib_nle_item c){if(!mulib_nle_timeline_valid(t)||!mulib_nle_item_valid(c)||t->count==t->capacity||mulib_nle_timeline_find(t,c.id)!=SIZE_MAX)return false;t->items[t->count++]=c;return true;}
static inline bool mulib_nle_timeline_remove(mulib_nle_timeline *t,uint64_t id){size_t i=mulib_nle_timeline_find(t,id);if(i==SIZE_MAX)return false;memmove(t->items+i,t->items+i+1,(t->count-i-1)*sizeof *t->items);t->count--;return true;}
#ifdef __cplusplus
}
#endif
#endif
