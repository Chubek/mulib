/** @file mulib-compute-buffer.h
 * Checked host buffer allocation and staging copies.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: CPU memory only, not device allocation.
 */
#ifndef MULIB_COMPUTE_BUFFER_H
#define MULIB_COMPUTE_BUFFER_H
#include "mulib-compute-core.h"
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
typedef struct {uint8_t *data;size_t size;} mulib_compute_buffer;
/* Zero-initialize b. It is a move-only owner, not a borrowed host view.
 * Free only allocations from this API; uploaded/downloaded spans have n bytes. */
static inline bool mulib_compute_buffer_alloc(mulib_compute_buffer *b,size_t n){if(!b||b->data||b->size||!n)return false;uint8_t *p=(uint8_t*)calloc(1,n);if(!p)return false;b->data=p;b->size=n;return true;}
static inline void mulib_compute_buffer_free(mulib_compute_buffer *b){if(b){free(b->data);memset(b,0,sizeof *b);}}
static inline bool mulib_compute_buffer_upload(mulib_compute_buffer b,size_t off,const void *p,size_t n){if(!b.data||off>b.size||n>b.size-off||(!p&&n))return false;if(n)memmove(b.data+off,p,n);return true;}
static inline bool mulib_compute_buffer_download(mulib_compute_buffer b,size_t off,void *p,size_t n){if(!b.data||off>b.size||n>b.size-off||(!p&&n))return false;if(n)memmove(p,b.data+off,n);return true;}
#ifdef __cplusplus
}
#endif
#endif
