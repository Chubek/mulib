/** @file mulib-nle-clip.h
 * Clip source mapping and bounded slip.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NLE_CLIP_H
#define MULIB_NLE_CLIP_H
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
static inline bool mulib_nle_clip_source(mulib_nle_item c,int64_t tick,int64_t *source){if(!source||!mulib_nle_item_valid(c)||tick<c.start||tick-c.start>=c.length)return false;return mulib_media_i64_add(c.source,tick-c.start,source);}
static inline bool mulib_nle_clip_slip(mulib_nle_item *c,int64_t delta,int64_t asset_length){if(!c||!mulib_nle_item_valid(*c))return false;int64_t s;if(!mulib_media_i64_add(c->source,delta,&s)||s<0||asset_length<c->length||s>asset_length-c->length)return false;c->source=s;return true;}
#ifdef __cplusplus
}
#endif
#endif
