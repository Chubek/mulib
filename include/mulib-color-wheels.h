/** @file mulib-color-wheels.h
 * Luminance-weighted three-way grading offsets.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Simple overlapping weights, not a Resolve-compatible wheel model.
 */
#ifndef MULIB_COLOR_WHEELS_H
#define MULIB_COLOR_WHEELS_H
#include "mulib-img-core.h"
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
static inline mulib_img_rgba mulib_color_wheels_apply(mulib_img_rgba c,const float shadow[3],const float mid[3],const float high[3]){if(!shadow||!mid||!high)return c;float y=mulib_media_clampf(mulib_img_luma(c),0,1),a=(1-y)*(1-y),b=2*y*(1-y),d=y*y;c.r+=a*shadow[0]+b*mid[0]+d*high[0];c.g+=a*shadow[1]+b*mid[1]+d*high[1];c.b+=a*shadow[2]+b*mid[2]+d*high[2];return c;}
#ifdef __cplusplus
}
#endif
#endif
