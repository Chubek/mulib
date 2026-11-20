/* MuLIB: canvas-tile. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_TILE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_TILE_H
#define MULIB_CANVAS_TILE_H
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
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_canvas_tile_bounds(mulib_geom2_box viewport,double tile_size,int64_t *x0,int64_t *y0,int64_t *x1,int64_t *y1);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_TILE) && !defined(MULIB_CANVAS_TILE_H_IMPLEMENTED)
#define MULIB_CANVAS_TILE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_canvas_tile_bounds(mulib_geom2_box viewport,double tile_size,int64_t *x0,int64_t *y0,int64_t *x1,int64_t *y1){if(!mulib_geom2_box_valid(viewport)||tile_size<=0||!isfinite(tile_size)||!x0||!y0||!x1||!y1)return 0;double a=floor(viewport.lo.x/tile_size),b=floor(viewport.lo.y/tile_size),c=floor(viewport.hi.x/tile_size),d=floor(viewport.hi.y/tile_size);if(a<-0x1p63||a>=0x1p63||b<-0x1p63||b>=0x1p63||c<-0x1p63||c>=0x1p63||d<-0x1p63||d>=0x1p63)return 0;*x0=(int64_t)a;*y0=(int64_t)b;*x1=(int64_t)c;*y1=(int64_t)d;return 1;}
#ifdef __cplusplus
}
#endif
#endif
