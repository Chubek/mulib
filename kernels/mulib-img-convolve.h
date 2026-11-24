/** @file mulib-img-convolve.h
 * General odd-kernel correlation with clamped boundaries.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Disjoint buffers; flip weights for true convolution.
 */
#ifndef MULIB_IMG_CONVOLVE_H
#define MULIB_IMG_CONVOLVE_H
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
/* k_count is the accessible float capacity; k must not overlap destination. */
static inline bool mulib_img_convolve(mulib_img_view d,mulib_img_view s,const float *k,size_t k_count,size_t w,size_t h){if(!mulib_img_same(d,s)||!mulib_img_disjoint(d,s)||!k||!w||!h||!(w&1)||!(h&1)||w>255||h>255||w*h>k_count||!mulib_img_disjoint_array(d,k,w*h,sizeof *k))return false;for(size_t i=0;i<w*h;i++)if(!isfinite(k[i]))return false;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){mulib_img_rgba a={0};for(size_t j=0;j<h;j++)for(size_t i=0;i<w;i++){mulib_img_rgba c=mulib_img_get(s,mulib_img_clamp_index((int64_t)x+(int64_t)i-(int64_t)(w/2),s.width),mulib_img_clamp_index((int64_t)y+(int64_t)j-(int64_t)(h/2),s.height));float z=k[j*w+i];a.r+=z*c.r;a.g+=z*c.g;a.b+=z*c.b;a.a+=z*c.a;}mulib_img_put(d,x,y,a);}return true;}
#ifdef __cplusplus
}
#endif
#endif
