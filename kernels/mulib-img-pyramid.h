/** @file mulib-img-pyramid.h
 * Half-resolution box-filter pyramid level.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not a Gaussian/Laplacian reconstruction pyramid.
 */
#ifndef MULIB_IMG_PYRAMID_H
#define MULIB_IMG_PYRAMID_H
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
static inline bool mulib_img_pyramid_down(mulib_img_view d,mulib_img_view s){if(!mulib_img_disjoint(d,s)||d.width!=s.width/2+s.width%2||d.height!=s.height/2+s.height%2)return false;for(size_t y=0;y<d.height;y++)for(size_t x=0;x<d.width;x++){mulib_img_rgba a={0};unsigned n=0;for(size_t j=0;j<2&&2*y+j<s.height;j++)for(size_t i=0;i<2&&2*x+i<s.width;i++){mulib_img_rgba c=mulib_img_get(s,2*x+i,2*y+j);a.r+=c.r;a.g+=c.g;a.b+=c.b;a.a+=c.a;n++;}a.r/=n;a.g/=n;a.b/=n;a.a/=n;mulib_img_put(d,x,y,a);}return true;}
#ifdef __cplusplus
}
#endif
#endif
