/** @file mulib-anim-blend.h
 * Normalized weighted scalar blending.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_ANIM_BLEND_H
#define MULIB_ANIM_BLEND_H
#include "mulib-mesh-core.h"
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
static inline bool mulib_anim_blend_weighted(const double *v,const double *w,size_t n,double *out){if(!v||!w||!n||!out)return false;long double sum=0,weight=0;for(size_t i=0;i<n;i++){if(!isfinite(v[i])||!isfinite(w[i])||w[i]<0)return false;sum+=(long double)v[i]*w[i];weight+=w[i];}if(!(weight>0)||!isfinite((double)(sum/weight)))return false;*out=(double)(sum/weight);return true;}
#ifdef __cplusplus
}
#endif
#endif
