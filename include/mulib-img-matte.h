/** @file mulib-img-matte.h
 * Luminance-derived soft alpha matte.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_IMG_MATTE_H
#define MULIB_IMG_MATTE_H
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
static inline bool mulib_img_matte(mulib_img_view d,mulib_img_view s,float low,float high,bool invert){if(!mulib_img_same(d,s)||!mulib_img_pointwise_alias(d,s)||!isfinite(low)||!isfinite(high)||high<=low)return false;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){mulib_img_rgba c=mulib_img_get(s,x,y);float z=(float)fmax(0,fmin(((double)mulib_img_luma(c)-low)/((double)high-low),1));c.a*=invert?1-z:z;mulib_img_put(d,x,y,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
