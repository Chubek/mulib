/* MuLIB: place-floorplan. C11 single-header kernel.
 * Define MULIB_IMPL_PLACE_FLOORPLAN in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PLACE_FLOORPLAN_H
#define MULIB_PLACE_FLOORPLAN_H
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
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_place_floorplan(size_t count,const mulib_geom2_point *sizes,double aspect,double utilization,double *width,double *height);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PLACE_FLOORPLAN) && !defined(MULIB_PLACE_FLOORPLAN_H_IMPLEMENTED)
#define MULIB_PLACE_FLOORPLAN_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_place_floorplan(size_t count,const mulib_geom2_point *sizes,double aspect,double utilization,double *width,double *height){if(!count||!sizes||!width||!height||aspect<=0||utilization<=0||utilization>1)return 0;double area=0,max_w=0,max_h=0;for(size_t i=0;i<count;i++){if(!mulib_geom2_point_valid(sizes[i])||sizes[i].x<=0||sizes[i].y<=0)return 0;area+=sizes[i].x*sizes[i].y;max_w=MULIB_STORAGE_MAX(max_w,sizes[i].x);max_h=MULIB_STORAGE_MAX(max_h,sizes[i].y);}*width=MULIB_STORAGE_MAX(max_w,sqrt(area*aspect/utilization));*height=MULIB_STORAGE_MAX(max_h,sqrt(area/(aspect*utilization)));return isfinite(*width+*height);}
#ifdef __cplusplus
}
#endif
#endif
