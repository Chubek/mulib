/** @file mulib-media-core.h
 * Common checked arithmetic, status and byte views.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_MEDIA_CORE_H
#define MULIB_MEDIA_CORE_H
#include "mulib-fraction.h"
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
typedef enum { MULIB_MEDIA_OK=0, MULIB_MEDIA_INVALID=-1, MULIB_MEDIA_RANGE=-2, MULIB_MEDIA_NOMEM=-3, MULIB_MEDIA_UNSUPPORTED=-4, MULIB_MEDIA_AGAIN=-5, MULIB_MEDIA_EOF=-6, MULIB_MEDIA_IO=-7, MULIB_MEDIA_CANCELLED=-8 } mulib_media_status;
/* Same representation as the canonical fraction API. Timebases additionally
 * require a strictly positive numerator and denominator. No fraction
 * implementation/link dependency is needed for this type alias. */
typedef mulib_fraction_t mulib_media_rational;
typedef struct { const uint8_t *data; size_t size; } mulib_media_bytes;
typedef struct { uint8_t *data; size_t size; } mulib_media_mutbytes;
static inline bool mulib_media_size_mul(size_t a,size_t b,size_t *out){if(!out||(b&&a>SIZE_MAX/b))return false; *out=a*b;return true;}
static inline bool mulib_media_size_add(size_t a,size_t b,size_t *out){if(!out||a>SIZE_MAX-b)return false;*out=a+b;return true;}
static inline bool mulib_media_i64_add(int64_t a,int64_t b,int64_t *out){if(!out||(b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))return false;*out=a+b;return true;}
static inline uint32_t mulib_media_be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static inline uint64_t mulib_media_be64(const uint8_t *p){return ((uint64_t)mulib_media_be32(p)<<32)|mulib_media_be32(p+4);}
static inline float mulib_media_clampf(float x,float a,float b){return x<a?a:x>b?b:x;}
static inline bool mulib_media_span_valid(mulib_media_bytes b){return b.data||!b.size;}
/* Conservative byte-range separation on uintptr_t-addressed platforms.
 * False also covers invalid spans/address wrap; no pointer ordering or
 * subtraction between unrelated C objects is performed. Zero spans may be NULL.
 * Caller must still provide live objects of at least the stated byte lengths. */
static inline bool mulib_media_ranges_disjoint(const void *a,size_t an,const void *b,size_t bn){
 if((!a&&an)||(!b&&bn))return false;
 if(!an||!bn)return true;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return false;
 return x+an<=y||y+bn<=x;
}
#ifdef __cplusplus
}
#endif
#endif
