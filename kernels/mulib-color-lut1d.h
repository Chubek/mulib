/** @file mulib-color-lut1d.h
 * Clamped linear 1D LUT interpolation.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_COLOR_LUT1D_H
#define MULIB_COLOR_LUT1D_H
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
/* n accessible floats; finite LUT entries required. Invalid inputs return NaN.
 * Double interpolation avoids overflow in subtraction of finite HDR endpoints. */
static inline float mulib_color_lut1d_eval(const float *a,size_t n,float x){size_t bytes;if(!a||!n||!isfinite(x)||!mulib_img_memory_span(a,n,sizeof *a,&bytes))return NAN;if(n==1)return a[0];double t=mulib_media_clampf(x,0,1)*(double)(n-1);size_t i=(size_t)t;if(i>=n-1)return a[n-1];return (float)((double)a[i]+((double)a[i+1]-a[i])*(t-i));}
#ifdef __cplusplus
}
#endif
#endif
