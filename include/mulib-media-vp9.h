/** @file mulib-media-vp9.h
 * VP9 uncompressed frame prefix parsing.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Does not validate full frame or decode pixels.
 */
#ifndef MULIB_MEDIA_VP9_H
#define MULIB_MEDIA_VP9_H
#include "mulib-media-core.h"
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
typedef struct {unsigned profile;bool show_existing,keyframe,show_frame,error_resilient;unsigned existing_index;} mulib_media_vp9_header;
/* Read up to 24 MSB-first bits. pos is in bits, b.size in bytes.
 * Cursor and output remain unchanged on a truncated/overflowing read. */
static inline bool mulib_media_vp9_bits(mulib_media_bytes b,size_t *pos,unsigned n,unsigned *out){if(!pos||!out||!b.data||n>24||n>SIZE_MAX-*pos)return false;size_t p=*pos;unsigned v=0;for(unsigned i=0;i<n;i++){if(p/8>=b.size)return false;v=(v<<1)|((b.data[p/8]>>(7-p%8))&1);p++;}*pos=p;*out=v;return true;}
static inline bool mulib_media_vp9_parse(mulib_media_bytes b,mulib_media_vp9_header *out){if(!out)return false;size_t p=0;unsigned x,y;mulib_media_vp9_header h={0};if(!mulib_media_vp9_bits(b,&p,2,&x)||x!=2||!mulib_media_vp9_bits(b,&p,1,&x)||!mulib_media_vp9_bits(b,&p,1,&y))return false;h.profile=x+2*y;if(h.profile==3&&(!mulib_media_vp9_bits(b,&p,1,&x)||x))return false;if(!mulib_media_vp9_bits(b,&p,1,&x))return false;h.show_existing=x!=0;if(h.show_existing){if(!mulib_media_vp9_bits(b,&p,3,&h.existing_index))return false;}else{if(!mulib_media_vp9_bits(b,&p,1,&x))return false;h.keyframe=!x;if(!mulib_media_vp9_bits(b,&p,1,&x)||!mulib_media_vp9_bits(b,&p,1,&y))return false;h.show_frame=x!=0;h.error_resilient=y!=0;}*out=h;return true;}
#ifdef __cplusplus
}
#endif
#endif
