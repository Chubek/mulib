/** @file mulib-nle-ripple.h
 * Validate-then-apply ripple shift.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Caller resolves overlaps and locked tracks.
 */
#ifndef MULIB_NLE_RIPPLE_H
#define MULIB_NLE_RIPPLE_H
#include "mulib-nle-timeline.h"
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
static inline bool mulib_nle_ripple_shift(mulib_nle_timeline *t,uint64_t track,int64_t from,int64_t delta){if(!mulib_nle_timeline_valid(t))return false;for(size_t i=0;i<t->count;i++){mulib_nle_item c=t->items[i];if((!track||c.track==track)&&c.start>=from){if(!mulib_media_i64_add(c.start,delta,&c.start)||!mulib_nle_item_valid(c))return false;}}for(size_t i=0;i<t->count;i++)if((!track||t->items[i].track==track)&&t->items[i].start>=from)t->items[i].start+=delta;return true;}
#ifdef __cplusplus
}
#endif
#endif
