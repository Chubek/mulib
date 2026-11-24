/** @file mulib-img-blend.h
 * Straight-alpha source-over compositing and blend modes.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_IMG_BLEND_H
#define MULIB_IMG_BLEND_H
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
typedef enum {MULIB_IMG_OVER,MULIB_IMG_MULTIPLY,MULIB_IMG_SCREEN,MULIB_IMG_ADD} mulib_img_blend_mode;
static inline float mulib_img_blend_channel(float a,float b,mulib_img_blend_mode m){return m==MULIB_IMG_MULTIPLY?a*b:m==MULIB_IMG_SCREEN?a+b-a*b:m==MULIB_IMG_ADD?a+b:b;}
static inline mulib_img_rgba mulib_img_blend_pixel(mulib_img_rgba a,mulib_img_rgba b,mulib_img_blend_mode m){float x=mulib_media_clampf(a.a,0,1),y=mulib_media_clampf(b.a,0,1),z=x+y-x*y;mulib_img_rgba c={0};c.a=z;if(z>0){c.r=(x*(1-y)*a.r+y*(1-x)*b.r+x*y*mulib_img_blend_channel(a.r,b.r,m))/z;c.g=(x*(1-y)*a.g+y*(1-x)*b.g+x*y*mulib_img_blend_channel(a.g,b.g,m))/z;c.b=(x*(1-y)*a.b+y*(1-x)*b.b+x*y*mulib_img_blend_channel(a.b,b.b,m))/z;}return c;}
static inline bool mulib_img_blend(mulib_img_view d,mulib_img_view a,mulib_img_view b,mulib_img_blend_mode m){if(!mulib_img_same(d,a)||!mulib_img_same(d,b)||!mulib_img_pointwise_alias(d,a)||!mulib_img_pointwise_alias(d,b)||m<MULIB_IMG_OVER||m>MULIB_IMG_ADD)return false;for(size_t y=0;y<d.height;y++)for(size_t x=0;x<d.width;x++)mulib_img_put(d,x,y,mulib_img_blend_pixel(mulib_img_get(a,x,y),mulib_img_get(b,x,y),m));return true;}
#ifdef __cplusplus
}
#endif
#endif
