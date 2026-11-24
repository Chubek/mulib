/** @file mulib-texglue.h
 * @brief TeX-like natural size, stretch and shrink plus proportional adjustment.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXGLUE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texglue.md for scope and limitations.
 */
#ifndef MULIB_TEXGLUE_H
#define MULIB_TEXGLUE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_texglue_glue { int64_t natural,stretch,shrink;unsigned stretch_order,shrink_order; } mulib_texglue_glue_t;
int mulib_texglue_adjust(const mulib_texglue_glue_t *items,size_t count,int64_t desired,int64_t *sizes);
int mulib_texglue_sum(const mulib_texglue_glue_t *items,size_t count,int64_t *natural);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXGLUE_H */
#if defined(MULIB_IMPL_TEXGLUE) && !defined(MULIB_TEXGLUE_H_IMPLEMENTED)
#define MULIB_TEXGLUE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texglue_sum(const mulib_texglue_glue_t *a,size_t n,int64_t *total){if(!total||(!a&&n))return 0;int64_t s=0;for(size_t i=0;i<n;i++){if(a[i].natural>0&&s>INT64_MAX-a[i].natural)return 0;if(a[i].natural<0&&s<INT64_MIN-a[i].natural)return 0;s+=a[i].natural;}*total=s;return 1;}
int mulib_texglue_adjust(const mulib_texglue_glue_t *a,size_t n,int64_t desired,int64_t *sizes){if((!a&&n)||(!sizes&&n))return 0;int64_t base=0;if(!mulib_texglue_sum(a,n,&base))return 0;long double delta=(long double)desired-base,weight=0;unsigned order=0;int expand=delta>=0;
 for(size_t i=0;i<n;i++){int64_t w=expand?a[i].stretch:a[i].shrink;unsigned ord=expand?a[i].stretch_order:a[i].shrink_order;if(w<0)return 0;if(w>0&&ord>order)order=ord;}
 for(size_t i=0;i<n;i++){unsigned ord=expand?a[i].stretch_order:a[i].shrink_order;if(ord==order)weight+=(long double)(expand?a[i].stretch:a[i].shrink);}
 if(weight==0&&delta!=0)return 0;
 for(size_t i=0;i<n;i++){int64_t w=expand?a[i].stretch:a[i].shrink;unsigned ord=expand?a[i].stretch_order:a[i].shrink_order;long double val=(long double)a[i].natural+(ord==order&&weight>0?delta*w/weight:0);if(val>INT64_MAX||val<INT64_MIN)return 0;sizes[i]=(int64_t)val;}
 return 1;}
#ifdef __cplusplus
}
#endif
#endif
