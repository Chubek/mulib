/** @file mulib-media-convert.h
 * RGB, gray, float and limited-range BT.709 YUV420 to RGBA8.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Disjoint source/destination; no transfer conversion.
 */
#ifndef MULIB_MEDIA_CONVERT_H
#define MULIB_MEDIA_CONVERT_H
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
static inline uint8_t mulib_media_convert_u8(float a){return (uint8_t)(mulib_media_clampf(isfinite(a)?a:0,0,1)*255.0f+0.5f);}
static inline bool mulib_media_convert_rgba8(mulib_media_frame *d,const mulib_media_frame *s){if(!mulib_media_frame_valid(d)||!mulib_media_frame_valid(s)||d->format!=MULIB_MEDIA_RGBA8||d->width!=s->width||d->height!=s->height)return false;if(s->format==d->format)return mulib_media_frame_copy(d,s);if(!mulib_media_frame_disjoint(d,s))return false;for(size_t y=0;y<s->height;y++)for(size_t x=0;x<s->width;x++){const uint8_t *q=s->data[0]+y*s->stride[0];uint8_t *p=d->data[0]+y*d->stride[0]+4*x;p[3]=255;if(s->format==MULIB_MEDIA_RGB8){p[0]=q[3*x];p[1]=q[3*x+1];p[2]=q[3*x+2];}else if(s->format==MULIB_MEDIA_GRAY8){p[0]=p[1]=p[2]=q[x];}else if(s->format==MULIB_MEDIA_RGBAF32){float v[4];memcpy(v,q+16*x,16);for(unsigned k=0;k<4;k++)p[k]=mulib_media_convert_u8(v[k]);}else{float yy=((float)q[x]-16)/219,u=((float)s->data[1][y/2*s->stride[1]+x/2]-128)/224,v=((float)s->data[2][y/2*s->stride[2]+x/2]-128)/224;p[0]=mulib_media_convert_u8(yy+1.5748f*v);p[1]=mulib_media_convert_u8(yy-.187324f*u-.468124f*v);p[2]=mulib_media_convert_u8(yy+1.8556f*u);}}d->pts=s->pts;d->timebase=s->timebase;return true;}
#ifdef __cplusplus
}
#endif
#endif
