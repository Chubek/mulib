/** @file mulib-img-warp.h
 * Displacement map warp; field R/G are source offsets in pixels.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Clamp border; disjoint buffers.
 */
#ifndef MULIB_IMG_WARP_H
#define MULIB_IMG_WARP_H
#include "mulib-img-core.h"
#include "mulib-img-sampler.h"
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
static inline bool mulib_img_warp(mulib_img_view d,mulib_img_view s,mulib_img_view field){if(!mulib_img_same(d,field)||!mulib_img_disjoint(d,s)||!mulib_img_disjoint(d,field))return false;for(size_t y=0;y<d.height;y++)for(size_t x=0;x<d.width;x++){mulib_img_rgba f=mulib_img_get(field,x,y);mulib_img_put(d,x,y,mulib_img_sample(s,(double)x+f.r,(double)y+f.g,MULIB_IMG_BILINEAR));}return true;}
#ifdef __cplusplus
}
#endif
#endif
