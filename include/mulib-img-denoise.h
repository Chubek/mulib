/** @file mulib-img-denoise.h
 * Spatial bilateral RGB denoising.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No temporal denoise.
 */
#ifndef MULIB_IMG_DENOISE_H
#define MULIB_IMG_DENOISE_H
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
static inline bool mulib_img_denoise_bilateral(mulib_img_view d,mulib_img_view s,unsigned radius,double space,double range){if(!mulib_img_same(d,s)||!mulib_img_disjoint(d,s)||radius>16||!isfinite(space)||!isfinite(range)||space<=0||range<=0)return false;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){mulib_img_rgba c=mulib_img_get(s,x,y),z={0};double total=0;for(int j=-(int)radius;j<=(int)radius;j++)for(int i=-(int)radius;i<=(int)radius;i++){mulib_img_rgba a=mulib_img_get(s,mulib_img_clamp_index((int64_t)x+i,s.width),mulib_img_clamp_index((int64_t)y+j,s.height));double r=((double)a.r-c.r)/range,g=((double)a.g-c.g)/range,b=((double)a.b-c.b)/range,xx=i/space,yy=j/space,w=exp(-.5*(xx*xx+yy*yy+r*r+g*g+b*b));z.r+=(float)(w*a.r);z.g+=(float)(w*a.g);z.b+=(float)(w*a.b);total+=w;}if(total>0){z.r/=(float)total;z.g/=(float)total;z.b/=(float)total;}else z=c;z.a=c.a;mulib_img_put(d,x,y,z);}return true;}
#ifdef __cplusplus
}
#endif
#endif
