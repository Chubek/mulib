/** @file mulib-img-core.h
 * Float RGBA strided image view and pixel math.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: get/put are unchecked internal primitives; validate and bound indices first.
 * Linear light, straight alpha; callers premultiply before filtering transparency.
 * Pixels/parameters must be finite unless an operation explicitly says otherwise.
 * HDR is not clipped; arithmetic may overflow to Inf, and subnormals are not flushed.
 */
#ifndef MULIB_IMG_CORE_H
#define MULIB_IMG_CORE_H
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
typedef struct {float r,g,b,a;} mulib_img_rgba;
/* stride and capacity are in float elements, NOT bytes. The caller must provide
 * an actual accessible allocation matching capacity; validation cannot prove it. */
typedef struct {float *data;size_t width,height,stride,capacity;} mulib_img_view;
static inline bool mulib_img_memory_span(const void *p,size_t n,size_t element,size_t *bytes){
    return bytes&&(!n||p)&&mulib_media_size_mul(n,element,bytes)&&
        *bytes<=UINTPTR_MAX-(uintptr_t)p;
}
/* Conservative enclosing byte ranges, including padding. Invalid spans fail. */
static inline bool mulib_img_memory_disjoint(const void *a,size_t an,size_t ae,const void *b,size_t bn,size_t be){
    size_t ab,bb;
    if(!mulib_img_memory_span(a,an,ae,&ab)||!mulib_img_memory_span(b,bn,be,&bb))return false;
    return !ab||!bb||(uintptr_t)a+ab<=(uintptr_t)b||(uintptr_t)b+bb<=(uintptr_t)a;
}
static inline bool mulib_img_span(mulib_img_view v,size_t *elements){
    size_t row,bytes;
    return elements&&v.data&&v.width&&v.height&&v.width<=INT_MAX&&v.height<=INT_MAX&&
        mulib_media_size_mul(v.width,4,&row)&&v.stride>=row&&
        mulib_media_size_mul(v.height-1,v.stride,elements)&&
        mulib_media_size_add(*elements,row,elements)&&*elements<=v.capacity&&
        mulib_img_memory_span(v.data,*elements,sizeof(float),&bytes);
}
static inline bool mulib_img_valid(mulib_img_view v){size_t n;return mulib_img_span(v,&n);}
static inline bool mulib_img_disjoint(mulib_img_view a,mulib_img_view b){
    size_t an,bn;
    return mulib_img_span(a,&an)&&mulib_img_span(b,&bn)&&
        mulib_img_memory_disjoint(a.data,an,sizeof(float),b.data,bn,sizeof(float));
}
/* Pointwise operations allow exactly the same mapping, but not shifted aliases. */
static inline bool mulib_img_pointwise_alias(mulib_img_view a,mulib_img_view b){
    return mulib_img_valid(a)&&mulib_img_valid(b)&&
        ((a.data==b.data&&a.width==b.width&&a.height==b.height&&a.stride==b.stride)||
         mulib_img_disjoint(a,b));
}
static inline bool mulib_img_disjoint_array(mulib_img_view v,const void *p,size_t n,size_t element){
    size_t vn;
    return mulib_img_span(v,&vn)&&mulib_img_memory_disjoint(v.data,vn,sizeof(float),p,n,element);
}
static inline mulib_img_rgba mulib_img_get(mulib_img_view v,size_t x,size_t y){float *p=v.data+y*v.stride+4*x;mulib_img_rgba c={p[0],p[1],p[2],p[3]};return c;}
static inline void mulib_img_put(mulib_img_view v,size_t x,size_t y,mulib_img_rgba c){float *p=v.data+y*v.stride+4*x;p[0]=c.r;p[1]=c.g;p[2]=c.b;p[3]=c.a;}
static inline bool mulib_img_same(mulib_img_view a,mulib_img_view b){return mulib_img_valid(a)&&mulib_img_valid(b)&&a.width==b.width&&a.height==b.height;}
static inline float mulib_img_luma(mulib_img_rgba a){return .2126f*a.r+.7152f*a.g+.0722f*a.b;}
static inline mulib_img_rgba mulib_img_lerp(mulib_img_rgba a,mulib_img_rgba b,float t){mulib_img_rgba c={a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,a.a+(b.a-a.a)*t};return c;}
/* n must be positive for image access; n==0 returns 0, never SIZE_MAX. */
static inline size_t mulib_img_clamp_index(int64_t x,size_t n){return !n||x<0?0:(uint64_t)x>=n?n-1:(size_t)x;}
#ifdef __cplusplus
}
#endif
#endif
