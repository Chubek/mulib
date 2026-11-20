/** @file mulib-img-transform.h
 * Inverse homography image transformation with transparent exterior.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Disjoint buffers.
 */
#ifndef MULIB_IMG_TRANSFORM_H
#define MULIB_IMG_TRANSFORM_H
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
/* inv points to nine accessible doubles and must not overlap d. */
static inline bool mulib_img_transform(mulib_img_view d,mulib_img_view s,const double inv[9],mulib_img_sampler_mode mode){if(!mulib_img_disjoint(d,s)||!inv||!mulib_img_disjoint_array(d,inv,9,sizeof *inv)||mode<MULIB_IMG_NEAREST||mode>MULIB_IMG_LANCZOS3)return false;for(int i=0;i<9;i++)if(!isfinite(inv[i]))return false;for(size_t y=0;y<d.height;y++)for(size_t x=0;x<d.width;x++){double w=inv[6]*x+inv[7]*y+inv[8];mulib_img_rgba c={0};if(fabs(w)>1e-12){double u=(inv[0]*x+inv[1]*y+inv[2])/w,v=(inv[3]*x+inv[4]*y+inv[5])/w;if(u>=0&&v>=0&&u<=s.width-1&&v<=s.height-1)c=mulib_img_sample(s,u,v,mode);}mulib_img_put(d,x,y,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
