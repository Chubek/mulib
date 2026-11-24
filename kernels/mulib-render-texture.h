/** @file mulib-render-texture.h
 * Bilinear sampling with mip-level interpolation and repeat coordinates.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Caller generates mip levels; repeat seams use clamped bilinear.
 */
#ifndef MULIB_RENDER_TEXTURE_H
#define MULIB_RENDER_TEXTURE_H
#include "mulib-mesh-core.h"
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
static inline mulib_img_rgba mulib_render_texture_sample(const mulib_img_view *levels,size_t n,double u,double v,double lod){mulib_img_rgba z={0};if(!levels||!n||n>SIZE_MAX/sizeof *levels||!isfinite(u)||!isfinite(v)||!isfinite(lod))return z;lod=fmax(0,lod);size_t i;double fraction;if(lod>=(double)(n-1)){i=n-1;fraction=0;}else{i=(size_t)lod;if(i>=n)return z;fraction=lod-i;}size_t j=i+1<n?i+1:i;if(!mulib_img_valid(levels[i])||!mulib_img_valid(levels[j]))return z;u-=floor(u);v-=floor(v);mulib_img_rgba a=mulib_img_sample(levels[i],u*(levels[i].width-1),v*(levels[i].height-1),MULIB_IMG_BILINEAR),b=mulib_img_sample(levels[j],u*(levels[j].width-1),v*(levels[j].height-1),MULIB_IMG_BILINEAR);return mulib_img_lerp(a,b,(float)fraction);}
#ifdef __cplusplus
}
#endif
#endif
