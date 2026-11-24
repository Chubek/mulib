/** @file mulib-color-tonemap.h
 * Reinhard and ACES-inspired fitted filmic curves.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not an official ACES rendering transform.
 */
#ifndef MULIB_COLOR_TONEMAP_H
#define MULIB_COLOR_TONEMAP_H
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
/* NaN propagates. Positive infinity maps to 1, nonpositive input to 0. */
static inline float mulib_color_tonemap_reinhard(float x){return isnan(x)?NAN:x<=0?0:isinf(x)?1:x/(1+x);}
static inline float mulib_color_tonemap_acesfit(float x){if(isnan(x))return NAN;x=fmaxf(x,0);if(x>1e10f)return 1;return mulib_media_clampf(x*(2.51f*x+.03f)/(x*(2.43f*x+.59f)+.14f),0,1);}
static inline float mulib_color_tonemap_exposed(float x,float exposure,bool aces){
    if(isnan(x)||isnan(exposure))return NAN;
    double v=x==0?0:(double)x*exp2((double)exposure);
    float z=v>FLT_MAX?INFINITY:v<-FLT_MAX?-INFINITY:(float)v;
    return aces?mulib_color_tonemap_acesfit(z):mulib_color_tonemap_reinhard(z);
}
static inline mulib_img_rgba mulib_color_tonemap_pixel(mulib_img_rgba c,float exposure,bool aces){c.r=mulib_color_tonemap_exposed(c.r,exposure,aces);c.g=mulib_color_tonemap_exposed(c.g,exposure,aces);c.b=mulib_color_tonemap_exposed(c.b,exposure,aces);return c;}
#ifdef __cplusplus
}
#endif
#endif
