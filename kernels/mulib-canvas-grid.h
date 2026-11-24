/* MuLIB: canvas-grid. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_GRID in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_GRID_H
#define MULIB_CANVAS_GRID_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_canvas_grid_step(double zoom,double min_pixels);
size_t mulib_canvas_grid_lines(double lo,double hi,double step,double *out,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_GRID) && !defined(MULIB_CANVAS_GRID_H_IMPLEMENTED)
#define MULIB_CANVAS_GRID_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_canvas_grid_step(double zoom,double min_pixels){if(zoom<=0||min_pixels<=0||!isfinite(zoom+min_pixels))return NAN;double raw=min_pixels/zoom,power=pow(10,floor(log10(raw))),d=raw/power;return power*(d<=1?1:d<=2?2:d<=5?5:10);}
size_t mulib_canvas_grid_lines(double lo,double hi,double step,double *out,size_t capacity){if(!isfinite(lo+hi+step)||hi<lo||step<=0)return MULIB_STORAGE_NONE;double first=ceil(lo/step)*step,nn=floor((hi-first)/step)+1;if(nn<=0)return 0;if(nn>=(double)SIZE_MAX)return MULIB_STORAGE_NONE;size_t n=(size_t)nn;for(size_t i=0;out&&i<n&&i<capacity;i++)out[i]=first+i*step;return n;}
#ifdef __cplusplus
}
#endif
#endif
