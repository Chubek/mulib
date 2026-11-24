/** @file mulib-media-scale.h
 * Nearest-neighbor RGBA8 scale.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Requires disjoint buffers; use img-resample for high quality.
 */
#ifndef MULIB_MEDIA_SCALE_H
#define MULIB_MEDIA_SCALE_H
#include "mulib-media-core.h"
#include "mulib-media-frame.h"
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
static inline bool mulib_media_scale_rgba8(mulib_media_frame *d,const mulib_media_frame *s){if(!mulib_media_frame_valid(d)||!mulib_media_frame_valid(s)||d->format!=MULIB_MEDIA_RGBA8||s->format!=d->format)return false;if(d->width==s->width&&d->height==s->height)return mulib_media_frame_copy(d,s);if(!mulib_media_frame_disjoint(d,s))return false;for(size_t y=0;y<d->height;y++){long double fy=(long double)y*s->height/d->height;size_t sy=fy>=(long double)(s->height-1)?s->height-1:(size_t)fy;for(size_t x=0;x<d->width;x++){long double fx=(long double)x*s->width/d->width;size_t sx=fx>=(long double)(s->width-1)?s->width-1:(size_t)fx;memcpy(d->data[0]+y*d->stride[0]+4*x,s->data[0]+sy*s->stride[0]+4*sx,4);}}d->pts=s->pts;d->timebase=s->timebase;return true;}
#ifdef __cplusplus
}
#endif
#endif
