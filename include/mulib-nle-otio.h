/** @file mulib-nle-otio.h
 * OpenTimelineIO RationalTime numeric bridge.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No OTIO JSON schema serialization.
 */
#ifndef MULIB_NLE_OTIO_H
#define MULIB_NLE_OTIO_H
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
typedef struct {double value,rate;} mulib_nle_otio_time;
/* Nearest tick, halfway cases away from zero. Output unchanged on failure. */
static inline bool mulib_nle_otio_time_to_ticks(mulib_nle_otio_time v,mulib_media_rational base,int64_t *out){if(!out||!isfinite(v.value)||!isfinite(v.rate)||v.rate<=0||base.num<=0||base.den<=0)return false;long double x=roundl((long double)v.value/v.rate*base.den/base.num);if(!isfinite(x)||x< -9223372036854775808.0L||x>=9223372036854775808.0L)return false;*out=(int64_t)x;return true;}
static inline bool mulib_nle_otio_ticks_to_time(int64_t t,mulib_media_rational base,double rate,mulib_nle_otio_time *out){if(!out||!isfinite(rate)||rate<=0||base.num<=0||base.den<=0)return false;long double v=(long double)t*base.num/base.den*rate;if(!isfinite(v)||v>DBL_MAX||v< -DBL_MAX)return false;out->value=(double)v;out->rate=rate;return true;}
#ifdef __cplusplus
}
#endif
#endif
