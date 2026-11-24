/** @file mulib-compute-image.h
 * Aligned-row host storage image allocation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No GPU texture memory.
 */
#ifndef MULIB_COMPUTE_IMAGE_H
#define MULIB_COMPUTE_IMAGE_H
#include "mulib-compute-core.h"
#include "mulib-compute-buffer.h"
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
typedef struct {mulib_compute_buffer buffer;size_t width,height,row_bytes,pixel_bytes;} mulib_compute_image;
/* Zero-initialize i; alloc/free own the host buffer. align aligns ROW PITCH
 * only, not the malloc base address. Sizes/pitches are bytes, dimensions pixels. */
static inline bool mulib_compute_image_alloc(mulib_compute_image *i,size_t w,size_t h,size_t pixel,size_t align){size_t row,n;if(!i||i->buffer.data||!w||!h||!pixel||!align||(align&(align-1))||!mulib_media_size_mul(w,pixel,&row)||row>SIZE_MAX-(align-1))return false;row=(row+align-1)&~(align-1);if(!mulib_media_size_mul(row,h,&n)||!mulib_compute_buffer_alloc(&i->buffer,n))return false;i->width=w;i->height=h;i->row_bytes=row;i->pixel_bytes=pixel;return true;}
static inline void mulib_compute_image_free(mulib_compute_image *i){if(i){mulib_compute_buffer_free(&i->buffer);memset(i,0,sizeof *i);}}
#ifdef __cplusplus
}
#endif
#endif
