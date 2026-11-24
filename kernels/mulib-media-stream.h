/** @file mulib-media-stream.h
 * Short-read and short-write handling for blocking streams.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_MEDIA_STREAM_H
#define MULIB_MEDIA_STREAM_H
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
typedef mulib_media_status (*mulib_media_stream_read_fn)(void *,void *,size_t,size_t *);
typedef mulib_media_status (*mulib_media_stream_write_fn)(void *,const void *,size_t,size_t *);
typedef mulib_media_status (*mulib_media_stream_seek_fn)(void *,int64_t);
typedef struct {void *ctx;mulib_media_stream_read_fn read;mulib_media_stream_write_fn write;mulib_media_stream_seek_fn seek;} mulib_media_stream;
static inline mulib_media_status mulib_media_stream_read_exact(mulib_media_stream s,void *p,size_t n){if(!s.read||(!p&&n))return MULIB_MEDIA_INVALID;size_t off=0;while(off<n){size_t got=0;mulib_media_status r=s.read(s.ctx,(uint8_t*)p+off,n-off,&got);if(r!=MULIB_MEDIA_OK)return r;if(!got)return MULIB_MEDIA_EOF;if(got>n-off)return MULIB_MEDIA_IO;off+=got;}return MULIB_MEDIA_OK;}
static inline mulib_media_status mulib_media_stream_write_all(mulib_media_stream s,const void *p,size_t n){if(!s.write||(!p&&n))return MULIB_MEDIA_INVALID;size_t off=0;while(off<n){size_t got=0;mulib_media_status r=s.write(s.ctx,(const uint8_t*)p+off,n-off,&got);if(r!=MULIB_MEDIA_OK)return r;if(!got||got>n-off)return MULIB_MEDIA_IO;off+=got;}return MULIB_MEDIA_OK;}
#ifdef __cplusplus
}
#endif
#endif
