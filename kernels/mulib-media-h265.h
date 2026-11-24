/** @file mulib-media-h265.h
 * Annex B HEVC NAL header extraction.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No HEVC encoder/decoder.
 */
#ifndef MULIB_MEDIA_H265_H
#define MULIB_MEDIA_H265_H
#include "mulib-media-core.h"
#include "mulib-media-h264.h"
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
static inline bool mulib_media_h265_nal(mulib_media_bytes b,size_t *off,mulib_media_bytes *nal,unsigned *type,unsigned *layer,unsigned *temporal){if(!off||!nal||!type||!layer||!temporal)return false;size_t p=0,i=mulib_media_h264_start(b,*off,&p);if(i==b.size)return false;i+=p;if(b.size-i<2)return false;size_t e=mulib_media_h264_start(b,i,&p),end=e;while(end>i+2&&!b.data[end-1])end--;if(end-i<2||(b.data[i]&128)||!(b.data[i+1]&7))return false;*type=(b.data[i]>>1)&63;*layer=((b.data[i]&1)<<5)|(b.data[i+1]>>3);*temporal=(b.data[i+1]&7)-1;nal->data=b.data+i;nal->size=end-i;*off=e;return true;}
#ifdef __cplusplus
}
#endif
#endif
