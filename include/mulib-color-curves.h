/** @file mulib-color-curves.h
 * Validated piecewise-linear grading curves.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No monotone cubic or cyclic hue interpolation.
 */
#ifndef MULIB_COLOR_CURVES_H
#define MULIB_COLOR_CURVES_H
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
typedef struct {float x,y;} mulib_color_curve_point;
static inline bool mulib_color_curves_eval(const mulib_color_curve_point *p,size_t n,float x,float *out){size_t bytes;if(!p||!n||!out||!isfinite(x)||!mulib_img_memory_span(p,n,sizeof *p,&bytes))return false;for(size_t i=0;i<n;i++)if(!isfinite(p[i].x)||!isfinite(p[i].y)||(i&&p[i].x<=p[i-1].x))return false;if(x<=p[0].x){*out=p[0].y;return true;}for(size_t i=1;i<n;i++)if(x<=p[i].x){double f=((double)x-p[i-1].x)/((double)p[i].x-p[i-1].x);*out=(float)((double)p[i-1].y+f*((double)p[i].y-p[i-1].y));return true;}*out=p[n-1].y;return true;}
#ifdef __cplusplus
}
#endif
#endif
