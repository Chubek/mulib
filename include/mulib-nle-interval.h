/** @file mulib-nle-interval.h
 * Half-open range intersection and bounded output query.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Linear scan; replace with augmented interval index for large timelines.
 */
#ifndef MULIB_NLE_INTERVAL_H
#define MULIB_NLE_INTERVAL_H
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
static inline bool mulib_nle_interval_overlap(int64_t a,int64_t b,int64_t c,int64_t d){return a<b&&c<d&&a<d&&c<b;}
static inline size_t mulib_nle_interval_query(const mulib_nle_timeline *t,uint64_t track,int64_t a,int64_t b,size_t *out,size_t cap){if(!mulib_nle_timeline_valid(t)||a>=b)return 0;size_t n=0;for(size_t i=0;i<t->count;i++){mulib_nle_item c=t->items[i];if(mulib_nle_item_valid(c)&&(!track||c.track==track)&&mulib_nle_interval_overlap(a,b,c.start,c.start+c.length)){if(out&&n<cap)out[n]=i;n++;}}return n;}
#ifdef __cplusplus
}
#endif
#endif
