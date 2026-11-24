/** @file mulib-img-tile.h
 * Zero-copy tile views and row-major tile enumeration.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_IMG_TILE_H
#define MULIB_IMG_TILE_H
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
typedef struct {size_t x,y,w,h;} mulib_img_rect;
static inline bool mulib_img_tile_view(mulib_img_view v,mulib_img_rect r,mulib_img_view *out){if(!out||!mulib_img_valid(v)||!r.w||!r.h||r.x>=v.width||r.y>=v.height||r.w>v.width-r.x||r.h>v.height-r.y)return false;size_t off=r.y*v.stride+4*r.x;mulib_img_view t={v.data+off,r.w,r.h,v.stride,v.capacity-off};*out=t;return true;}
static inline bool mulib_img_tile_next(mulib_img_view v,size_t edge,size_t *cursor,mulib_img_rect *r){if(!cursor||!r||!edge||!mulib_img_valid(v))return false;size_t nx=1+(v.width-1)/edge,ny=1+(v.height-1)/edge,total;if(!mulib_media_size_mul(nx,ny,&total)||*cursor>=total)return false;size_t x=(*cursor%nx)*edge,y=(*cursor/nx)*edge;r->x=x;r->y=y;r->w=v.width-x<edge?v.width-x:edge;r->h=v.height-y<edge?v.height-y:edge;(*cursor)++;return true;}
#ifdef __cplusplus
}
#endif
#endif
