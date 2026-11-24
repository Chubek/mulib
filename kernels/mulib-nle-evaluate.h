/** @file mulib-nle-evaluate.h
 * Visit active clips at a timeline tick.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Track compositing order is caller policy.
 */
#ifndef MULIB_NLE_EVALUATE_H
#define MULIB_NLE_EVALUATE_H
#include "mulib-nle-timeline.h"
#include "mulib-nle-clip.h"
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
typedef bool (*mulib_nle_evaluate_visit)(void*,const mulib_nle_item*,int64_t);
static inline bool mulib_nle_evaluate_at(const mulib_nle_timeline *t,int64_t tick,mulib_nle_evaluate_visit fn,void *ctx){if(!mulib_nle_timeline_valid(t)||!fn)return false;for(size_t i=0;i<t->count;i++){int64_t source;if(mulib_nle_clip_source(t->items[i],tick,&source)&&!fn(ctx,&t->items[i],source))return false;}return true;}
#ifdef __cplusplus
}
#endif
#endif
