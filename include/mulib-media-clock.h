/** @file mulib-media-clock.h
 * Anchored monotonic host-clock mapping (rate in ticks/ns).
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Caller supplies monotonic timestamps; no audio PLL.
 */
#ifndef MULIB_MEDIA_CLOCK_H
#define MULIB_MEDIA_CLOCK_H
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
typedef struct {int64_t anchor_pts,anchor_ns;double rate;bool paused;} mulib_media_clock;
/* rate is ticks/ns; conversion truncates toward zero and leaves output unchanged
 * on nonfinite or out-of-range results. Host timestamps must be monotonic. */
static inline bool mulib_media_clock_position(const mulib_media_clock *c,int64_t ns,int64_t *pts){if(!c||!pts||!isfinite(c->rate)||c->rate<=0)return false;if(c->paused){*pts=c->anchor_pts;return true;}long double x=(long double)c->anchor_pts+((long double)ns-c->anchor_ns)*c->rate;if(!isfinite(x)||x < -9223372036854775808.0L||x>=9223372036854775808.0L)return false;*pts=(int64_t)x;return true;}
static inline bool mulib_media_clock_pause(mulib_media_clock *c,int64_t ns,bool pause){int64_t p;if(!mulib_media_clock_position(c,ns,&p))return false;c->anchor_pts=p;c->anchor_ns=ns;c->paused=pause;return true;}
#ifdef __cplusplus
}
#endif
#endif
