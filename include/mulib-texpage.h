/** @file mulib-texpage.h
 * @brief Greedy bounded vertical page segmentation over box heights.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXPAGE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texpage.md for scope and limitations.
 */
#ifndef MULIB_TEXPAGE_H
#define MULIB_TEXPAGE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_texpage_pages { size_t *ends;size_t count; } mulib_texpage_pages_t;
int mulib_texpage_paginate(const double *heights,size_t count,double page_height,mulib_texpage_pages_t *out);
void mulib_texpage_destroy(mulib_texpage_pages_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXPAGE_H */
#if defined(MULIB_IMPL_TEXPAGE) && !defined(MULIB_TEXPAGE_H_IMPLEMENTED)
#define MULIB_TEXPAGE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texpage_paginate(const double *h,size_t n,double height,mulib_texpage_pages_t *out){if(!out||(!h&&n)||!isfinite(height)||height<=0||n>SIZE_MAX/sizeof(size_t))return 0;size_t *end=(size_t*)malloc((n?n:1)*sizeof(*end));if(!end)return 0;double used=0;size_t pages=0;for(size_t i=0;i<n;i++){if(!isfinite(h[i])||h[i]<0){free(end);return 0;}if(used>0&&used+h[i]>height){end[pages++]=i;used=0;}used+=h[i];}if(n)end[pages++]=n;out->ends=end;out->count=pages;return 1;}
void mulib_texpage_destroy(mulib_texpage_pages_t *p){if(p){free(p->ends);memset(p,0,sizeof(*p));}}
#ifdef __cplusplus
}
#endif
#endif
