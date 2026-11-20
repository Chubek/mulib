/** @file mulib-img-blur.h
 * Separable Gaussian blur with caller-provided scratch.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Scratch must not overlap either image; source/destination may overlap
 * because the complete source is consumed before the destination pass.
 */
#ifndef MULIB_IMG_BLUR_H
#define MULIB_IMG_BLUR_H
#include "mulib-img-core.h"
#include "mulib-img-convolve.h"
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
static inline bool mulib_img_blur_gaussian(mulib_img_view d,mulib_img_view tmp,mulib_img_view s,unsigned radius,double sigma){if(!mulib_img_same(d,s)||!mulib_img_same(tmp,s)||!mulib_img_disjoint(tmp,d)||!mulib_img_disjoint(tmp,s)||radius>127||!isfinite(sigma)||sigma<=0)return false;float k[255];double sum=0;for(unsigned i=0;i<2*radius+1;i++){double x=((double)i-radius)/sigma;k[i]=(float)exp(-.5*x*x);sum+=k[i];}if(!(sum>0))return false;for(unsigned i=0;i<2*radius+1;i++)k[i]/=(float)sum;return mulib_img_convolve(tmp,s,k,255,2*radius+1,1)&&mulib_img_convolve(d,tmp,k,255,1,2*radius+1);}
#ifdef __cplusplus
}
#endif
#endif
