/** @file mulib-texdim.h
 * @brief TeX-compatible scaled-point integer representation and overflow-checked arithmetic.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXDIM once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texdim.md for scope and limitations.
 */
#ifndef MULIB_TEXDIM_H
#define MULIB_TEXDIM_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int64_t mulib_texdim_sp_t;
#define MULIB_TEXDIM_SP_PER_PT 65536LL
int mulib_texdim_from_pt(double points,mulib_texdim_sp_t *out);
double mulib_texdim_to_pt(mulib_texdim_sp_t value);
int mulib_texdim_add(mulib_texdim_sp_t a,mulib_texdim_sp_t b,mulib_texdim_sp_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXDIM_H */
#if defined(MULIB_IMPL_TEXDIM) && !defined(MULIB_TEXDIM_H_IMPLEMENTED)
#define MULIB_TEXDIM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texdim_from_pt(double x,mulib_texdim_sp_t *o){if(!o||!isfinite(x)||x>(double)INT64_MAX/65536.0||x<(double)INT64_MIN/65536.0)return 0;double z=x*65536.0;*o=(int64_t)(z>=0?z+0.5:z-0.5);return 1;}
double mulib_texdim_to_pt(mulib_texdim_sp_t v){return (double)v/65536.0;}
int mulib_texdim_add(mulib_texdim_sp_t a,mulib_texdim_sp_t b,mulib_texdim_sp_t *o){if(!o||(b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))return 0;*o=a+b;return 1;}
#ifdef __cplusplus
}
#endif
#endif
