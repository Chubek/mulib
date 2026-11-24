/** @file mulib-media-bufferpool.h
 * Fixed-size reusable frame pool.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Single-threaded; all leases must be returned before destruction.
 */
#ifndef MULIB_MEDIA_BUFFERPOOL_H
#define MULIB_MEDIA_BUFFERPOOL_H
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
typedef struct {mulib_media_frame *frames;uint8_t *busy;size_t count;} mulib_media_bufferpool;
static inline void mulib_media_bufferpool_destroy(mulib_media_bufferpool *p){if(p){if(p->frames)for(size_t i=0;i<p->count;i++)mulib_media_frame_release(&p->frames[i]);free(p->frames);free(p->busy);memset(p,0,sizeof *p);}}
static inline bool mulib_media_bufferpool_init(mulib_media_bufferpool *p,size_t n,size_t w,size_t h,mulib_media_pixelformat f){if(!p||p->frames||p->busy||!n||n>SIZE_MAX/sizeof(mulib_media_frame))return false;mulib_media_bufferpool t={0};t.count=n;t.frames=(mulib_media_frame*)calloc(n,sizeof *t.frames);t.busy=(uint8_t*)calloc(n,1);if(!t.frames||!t.busy){mulib_media_bufferpool_destroy(&t);return false;}for(size_t i=0;i<n;i++)if(!mulib_media_frame_alloc(&t.frames[i],w,h,f)){mulib_media_bufferpool_destroy(&t);return false;}*p=t;return true;}
static inline mulib_media_frame *mulib_media_bufferpool_acquire(mulib_media_bufferpool *p){if(p&&p->frames&&p->busy)for(size_t i=0;i<p->count;i++)if(!p->busy[i]){p->busy[i]=1;return &p->frames[i];}return NULL;}
static inline bool mulib_media_bufferpool_return(mulib_media_bufferpool *p,mulib_media_frame *f){if(p&&p->frames&&p->busy)for(size_t i=0;i<p->count;i++)if(&p->frames[i]==f&&p->busy[i]){p->busy[i]=0;return true;}return false;}
#ifdef __cplusplus
}
#endif
#endif
