/** @file mulib-img-tracking.h
 * Track a set of points by block matching with confidence rejection.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No feature detection or subpixel refinement.
 */
#ifndef MULIB_IMG_TRACKING_H
#define MULIB_IMG_TRACKING_H
#include "mulib-img-core.h"
#include "mulib-img-opticalflow.h"
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
typedef struct {int x,y;double error;bool valid;} mulib_img_tracking_point;
/* n accessible points, disjoint from both images. Invalid call returns 0
 * without mutation; per-point failure clears valid. Caller bounds n/work. */
static inline size_t mulib_img_tracking_step(mulib_img_view a,mulib_img_view b,mulib_img_tracking_point *p,size_t n,int patch,int search,double threshold){if(!mulib_img_same(a,b)||!mulib_img_disjoint_array(a,p,n,sizeof *p)||!mulib_img_disjoint_array(b,p,n,sizeof *p)||patch<0||patch>16||search<0||search>64||!isfinite(threshold)||threshold<0)return 0;size_t good=0;for(size_t i=0;i<n;i++)if(p[i].valid){int dx,dy;double e;bool ok=mulib_img_opticalflow_block(a,b,p[i].x,p[i].y,patch,search,&dx,&dy,&e);if(!ok||e>threshold||(int64_t)p[i].x+dx<0||(int64_t)p[i].y+dy<0||(uint64_t)((int64_t)p[i].x+dx)>=b.width||(uint64_t)((int64_t)p[i].y+dy)>=b.height)p[i].valid=false;else{p[i].x+=dx;p[i].y+=dy;p[i].error=e;good++;}}return good;}
#ifdef __cplusplus
}
#endif
#endif
