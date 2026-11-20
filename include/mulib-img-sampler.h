/** @file mulib-img-sampler.h
 * Nearest, bilinear, Catmull-Rom and Lanczos3 samplers.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Clamp borders; straight alpha interpolation, premultiply if filtering transparent images.
 */
#ifndef MULIB_IMG_SAMPLER_H
#define MULIB_IMG_SAMPLER_H
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
typedef enum {MULIB_IMG_NEAREST,MULIB_IMG_BILINEAR,MULIB_IMG_BICUBIC,MULIB_IMG_LANCZOS3} mulib_img_sampler_mode;
static inline double mulib_img_sampler_weight(double x,mulib_img_sampler_mode m){x=fabs(x);if(m==MULIB_IMG_BICUBIC){if(x<1)return (1.5*x-2.5)*x*x+1;if(x<2)return ((-.5*x+2.5)*x-4)*x+2;return 0;}if(x>=3)return 0;if(x<1e-12)return 1;double p=3.141592653589793*x;return sin(p)/p*sin(p/3)/(p/3);}
static inline mulib_img_rgba mulib_img_sample(mulib_img_view v,double x,double y,mulib_img_sampler_mode m){mulib_img_rgba z={0};if(!mulib_img_valid(v)||!isfinite(x)||!isfinite(y))return z;x=fmax(0,fmin(x,(double)v.width-1));y=fmax(0,fmin(y,(double)v.height-1));int64_t ix=(int64_t)floor(x),iy=(int64_t)floor(y);if(m==MULIB_IMG_NEAREST)return mulib_img_get(v,(size_t)floor(x+.5),(size_t)floor(y+.5));if(m==MULIB_IMG_BILINEAR){size_t x1=mulib_img_clamp_index(ix+1,v.width),y1=mulib_img_clamp_index(iy+1,v.height);return mulib_img_lerp(mulib_img_lerp(mulib_img_get(v,ix,iy),mulib_img_get(v,x1,iy),(float)(x-ix)),mulib_img_lerp(mulib_img_get(v,ix,y1),mulib_img_get(v,x1,y1),(float)(x-ix)),(float)(y-iy));}if(m!=MULIB_IMG_BICUBIC&&m!=MULIB_IMG_LANCZOS3)return z;int r=m==MULIB_IMG_BICUBIC?2:3;double sum=0,rr=0,gg=0,bb=0,aa=0;for(int j=1-r;j<=r;j++)for(int i=1-r;i<=r;i++){double w=mulib_img_sampler_weight(x-ix-i,m)*mulib_img_sampler_weight(y-iy-j,m);mulib_img_rgba c=mulib_img_get(v,mulib_img_clamp_index(ix+i,v.width),mulib_img_clamp_index(iy+j,v.height));rr+=w*c.r;gg+=w*c.g;bb+=w*c.b;aa+=w*c.a;sum+=w;}if(fabs(sum)>1e-12){z.r=(float)(rr/sum);z.g=(float)(gg/sum);z.b=(float)(bb/sum);z.a=(float)(aa/sum);}return z;}
#ifdef __cplusplus
}
#endif
#endif
