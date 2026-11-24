/** @file mulib-media-h264.h
 * Annex B NAL iterator and emulation-prevention removal.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Bitstream tooling only; no H.264 encoder/decoder.
 */
#ifndef MULIB_MEDIA_H264_H
#define MULIB_MEDIA_H264_H
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
static inline size_t mulib_media_h264_start(mulib_media_bytes b,size_t i,size_t *prefix){if(!b.data||!prefix)return b.size;for(;i<b.size&&b.size-i>=3;i++)if(!b.data[i]&&!b.data[i+1]){if(b.data[i+2]==1){*prefix=3;return i;}if(b.size-i>=4&&!b.data[i+2]&&b.data[i+3]==1){*prefix=4;return i;}}return b.size;}
static inline bool mulib_media_h264_nal(mulib_media_bytes b,size_t *off,mulib_media_bytes *nal,unsigned *type){if(!off||!nal||!type)return false;size_t k=0,j=mulib_media_h264_start(b,*off,&k);if(j==b.size)return false;j+=k;if(j>=b.size)return false;size_t e=mulib_media_h264_start(b,j,&k),end=e;while(end>j&&!b.data[end-1])end--;if(end==j||(b.data[j]&128))return false;*type=b.data[j]&31;nal->data=b.data+j;nal->size=end-j;*off=e;return true;}
/* out may equal nal.data for in-place compaction; otherwise spans must be
 * disjoint. On malformed/capacity failure scratch may be partially written,
 * but written is unchanged. Counts are bytes. */
static inline bool mulib_media_h264_rbsp(mulib_media_bytes nal,mulib_media_mutbytes out,size_t *written){if(!written||!mulib_media_span_valid(nal)||(!out.data&&out.size)||(out.data!=nal.data&&!mulib_media_ranges_disjoint(nal.data,nal.size,out.data,out.size)))return false;size_t j=0;unsigned z=0;for(size_t i=0;i<nal.size;i++){uint8_t c=nal.data[i];if(z>=2&&c==3){if(i+1>=nal.size||nal.data[i+1]>3)return false;z=0;continue;}if(j==out.size)return false;out.data[j++]=c;z=c==0?(z<2?z+1:2):0;}*written=j;return true;}
#ifdef __cplusplus
}
#endif
#endif
