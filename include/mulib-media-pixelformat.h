/** @file mulib-media-pixelformat.h
 * Plane layout for packed and subsampled images.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_MEDIA_PIXELFORMAT_H
#define MULIB_MEDIA_PIXELFORMAT_H
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
typedef enum { MULIB_MEDIA_RGBA8, MULIB_MEDIA_RGB8, MULIB_MEDIA_GRAY8, MULIB_MEDIA_RGBAF32, MULIB_MEDIA_YUV420P8 } mulib_media_pixelformat;
/* RGBAF32 stores four native C floats per pixel (binary32 size/precision
 * required). Other C float layouts report the format unsupported. */
static inline unsigned mulib_media_pixelformat_planes(mulib_media_pixelformat f){if(f==MULIB_MEDIA_RGBAF32&&(sizeof(float)!=4||FLT_RADIX!=2||FLT_MANT_DIG!=24||FLT_MAX_EXP!=128))return 0;return f==MULIB_MEDIA_YUV420P8?3:(f>=MULIB_MEDIA_RGBA8&&f<=MULIB_MEDIA_RGBAF32?1:0);}
static inline bool mulib_media_pixelformat_layout(mulib_media_pixelformat f,size_t w,size_t h,unsigned p,size_t *row,size_t *rows){if(!row||!rows||!w||!h||p>=mulib_media_pixelformat_planes(f))return false;size_t b=f==MULIB_MEDIA_RGBAF32?16:f==MULIB_MEDIA_RGBA8?4:f==MULIB_MEDIA_RGB8?3:1;if(p){w=w/2+w%2;h=h/2+h%2;}if(!mulib_media_size_mul(w,b,row))return false;*rows=h;return true;}
#ifdef __cplusplus
}
#endif
#endif
