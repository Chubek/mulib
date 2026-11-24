/** @file mulib-render-denoise.h
 * Normal-guided 5x5 bilateral render denoising.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Normals in signed RGB, no temporal accumulation.
 */
#ifndef MULIB_RENDER_DENOISE_H
#define MULIB_RENDER_DENOISE_H
#include "mulib-mesh-core.h"
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
static inline bool mulib_render_denoise(mulib_img_view d,mulib_img_view s,mulib_img_view normal,double sigma){if(!mulib_img_same(d,s)||!mulib_img_same(s,normal)||d.data==s.data||d.data==normal.data||!isfinite(sigma)||sigma<=0)return false;for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){mulib_img_rgba c=mulib_img_get(s,x,y),n=mulib_img_get(normal,x,y),z={0};double wsum=0;for(int j=-2;j<=2;j++)for(int i=-2;i<=2;i++){size_t u=mulib_img_clamp_index((int64_t)x+i,s.width),v=mulib_img_clamp_index((int64_t)y+j,s.height);mulib_img_rgba a=mulib_img_get(s,u,v),b=mulib_img_get(normal,u,v);double r=(a.r-c.r)/sigma,g=(a.g-c.g)/sigma,k=(a.b-c.b)/sigma,w=exp(-.5*(r*r+g*g+k*k)-.25*(i*i+j*j))*pow(fmax(0,fmin(1,n.r*b.r+n.g*b.g+n.b*b.b)),8);z.r+=(float)(a.r*w);z.g+=(float)(a.g*w);z.b+=(float)(a.b*w);wsum+=w;}if(wsum>0){z.r/=(float)wsum;z.g/=(float)wsum;z.b/=(float)wsum;z.a=c.a;}else z=c;mulib_img_put(d,x,y,z);}return true;}
#ifdef __cplusplus
}
#endif
#endif
