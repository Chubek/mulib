/* MuLIB: pcb-pour. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_POUR in exactly one implementation translation unit.
 * Profile: Grid-sampled polygon copper fill with circular keepouts, not exact polygon clipping/thermals.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_POUR_H
#define MULIB_PCB_POUR_H
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
#include "mulib-num-core.h"
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {mulib_geom2_point center;double radius;} mulib_pcb_circle;
int mulib_pcb_pour(const mulib_geom2_point *poly,size_t n,mulib_geom2_point origin,double step,size_t w,size_t h,const mulib_pcb_circle *keep,size_t nk,unsigned char *mask);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_POUR) && !defined(MULIB_PCB_POUR_H_IMPLEMENTED)
#define MULIB_PCB_POUR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_pour(const mulib_geom2_point *poly,size_t n,mulib_geom2_point origin,double step,size_t w,size_t h,const mulib_pcb_circle *keep,size_t nk,unsigned char *mask){size_t count;if(!poly||n<3||!mask||!mulib_storage_size_mul(w,h,&count)||!isfinite(step)||step<=0||(!keep&&nk))return 0;for(size_t i=0;i<nk;i++)if(!mulib_geom2_point_valid(keep[i].center)||!isfinite(keep[i].radius)||keep[i].radius<0)return 0;for(size_t y=0;y<h;y++)for(size_t x=0;x<w;x++){mulib_geom2_point q={origin.x+(x+.5)*step,origin.y+(y+.5)*step};int ok=mulib_geom2_polygon_contains(poly,n,q);for(size_t i=0;i<nk&&ok;i++)if(mulib_geom2_distance(q,keep[i].center)<=keep[i].radius)ok=0;mask[y*w+x]=(unsigned char)ok;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
