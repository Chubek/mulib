/** @file mulib-img-morphology.h
 * Square alpha erosion and dilation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Disjoint buffers.
 */
#ifndef MULIB_IMG_MORPHOLOGY_H
#define MULIB_IMG_MORPHOLOGY_H
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
static inline bool mulib_img_morphology_alpha(mulib_img_view d,mulib_img_view s,unsigned radius,bool dilate){if(!mulib_img_same(d,s)||!mulib_img_disjoint(d,s)||radius>64)return false;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){mulib_img_rgba c=mulib_img_get(s,x,y);float z=c.a;for(int j=-(int)radius;j<=(int)radius;j++)for(int i=-(int)radius;i<=(int)radius;i++){float a=mulib_img_get(s,mulib_img_clamp_index((int64_t)x+i,s.width),mulib_img_clamp_index((int64_t)y+j,s.height)).a;z=dilate?fmaxf(z,a):fminf(z,a);}c.a=z;mulib_img_put(d,x,y,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
