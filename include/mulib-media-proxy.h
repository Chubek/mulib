/** @file mulib-media-proxy.h
 * Aspect-preserving in-memory proxy frame generation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No proxy media files or cache.
 */
#ifndef MULIB_MEDIA_PROXY_H
#define MULIB_MEDIA_PROXY_H
#include "mulib-media-core.h"
#include "mulib-media-scale.h"
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
static inline bool mulib_media_proxy_make(mulib_media_frame *d,const mulib_media_frame *s,size_t max_w,size_t max_h){if(!d||!mulib_media_frame_valid(s)||s->format!=MULIB_MEDIA_RGBA8||!max_w||!max_h)return false;long double f=fminl(1,fminl((long double)max_w/s->width,(long double)max_h/s->height));size_t w=(size_t)fmaxl(1,floorl(s->width*f)),h=(size_t)fmaxl(1,floorl(s->height*f));if(!mulib_media_frame_alloc(d,w,h,s->format))return false;if(!mulib_media_scale_rgba8(d,s)){mulib_media_frame_release(d);return false;}return true;}
#ifdef __cplusplus
}
#endif
#endif
