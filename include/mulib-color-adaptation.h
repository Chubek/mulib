/** @file mulib-color-adaptation.h
 * Bradford XYZ white-point adaptation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Supply normalized XYZ white points.
 */
#ifndef MULIB_COLOR_ADAPTATION_H
#define MULIB_COLOR_ADAPTATION_H
#include "mulib-img-core.h"
#include "mulib-color-primaries.h"
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
static inline bool mulib_color_adaptation_bradford(const double src[3],const double dst[3],mulib_color_mat3 *out){if(!src||!dst||!out)return false;mulib_color_mat3 b={{.8951,.2664,-.1614,-.7502,1.7135,.0367,.0389,-.0685,1.0296}},bi,d={{0}};double a[3],c[3];mulib_color_primaries_apply(b,src,a);mulib_color_primaries_apply(b,dst,c);for(int i=0;i<3;i++){if(!isfinite(a[i])||!isfinite(c[i])||fabs(a[i])<1e-12)return false;d.m[4*i]=c[i]/a[i];if(!isfinite(d.m[4*i]))return false;}if(!mulib_color_primaries_inverse(b,&bi))return false;mulib_color_mat3 result=mulib_color_primaries_mul(bi,mulib_color_primaries_mul(d,b));for(int i=0;i<9;i++)if(!isfinite(result.m[i]))return false;*out=result;return true;}
#ifdef __cplusplus
}
#endif
#endif
