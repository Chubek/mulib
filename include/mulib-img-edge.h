/** @file mulib-img-edge.h
 * Sobel gradient magnitude image.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No Canny hysteresis or Laplacian.
 */
#ifndef MULIB_IMG_EDGE_H
#define MULIB_IMG_EDGE_H
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
static inline bool mulib_img_edge_sobel(mulib_img_view d,mulib_img_view s){if(!mulib_img_same(d,s)||!mulib_img_disjoint(d,s))return false;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){float gx=0,gy=0;for(int j=-1;j<=1;j++)for(int i=-1;i<=1;i++){float l=mulib_img_luma(mulib_img_get(s,mulib_img_clamp_index((int64_t)x+i,s.width),mulib_img_clamp_index((int64_t)y+j,s.height)));gx+=l*i*(j?1:2);gy+=l*j*(i?1:2);}float z=hypotf(gx,gy)*.25f;mulib_img_rgba c={z,z,z,1};mulib_img_put(d,x,y,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
