/** @file mulib-texpenalty.h
 * @brief Clamp page-break penalties to configured window. Finite numeric inputs and documented scalar result.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXPENALTY once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texpenalty.md for scope and limitations.
 */
#ifndef MULIB_TEXPENALTY_H
#define MULIB_TEXPENALTY_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texpenalty_compute(double a, double b, double *result);
int mulib_texpenalty_clamp(double input, double lower, double upper, double *result);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXPENALTY_H */
#if defined(MULIB_IMPL_TEXPENALTY) && !defined(MULIB_TEXPENALTY_H_IMPLEMENTED)
#define MULIB_TEXPENALTY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texpenalty_compute(double a,double b,double *result){if(!result||!isfinite(a)||!isfinite(b))return 0;double v=(a+b);if(!isfinite(v))return 0;*result=v;return 1;}
int mulib_texpenalty_clamp(double input,double lower,double upper,double *result){if(!result||!isfinite(input)||!isfinite(lower)||!isfinite(upper)||lower>upper)return 0;*result=input<lower?lower:input>upper?upper:input;return 1;}
#ifdef __cplusplus
}
#endif
#endif
