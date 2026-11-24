/** @file mulib-img-resample.h
 * Center-aligned image reconstruction.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Disjoint buffers; no scale-dependent antialiasing for strong minification.
 */
#ifndef MULIB_IMG_RESAMPLE_H
#define MULIB_IMG_RESAMPLE_H
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
static inline bool mulib_img_resample(mulib_img_view d,mulib_img_view s,mulib_img_sampler_mode m){if(!mulib_img_disjoint(d,s)||m<MULIB_IMG_NEAREST||m>MULIB_IMG_LANCZOS3)return false;for(size_t y=0;y<d.height;y++)for(size_t x=0;x<d.width;x++)mulib_img_put(d,x,y,mulib_img_sample(s,((double)x+.5)*s.width/d.width-.5,((double)y+.5)*s.height/d.height-.5,m));return true;}
#ifdef __cplusplus
}
#endif
#endif
