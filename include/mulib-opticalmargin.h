/** @file mulib-opticalmargin.h
 * @brief Calculate optical margin inset. Finite numeric inputs and documented scalar result.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_OPTICALMARGIN once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-opticalmargin.md for scope and limitations.
 */
#ifndef MULIB_OPTICALMARGIN_H
#define MULIB_OPTICALMARGIN_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_opticalmargin_compute(double a, double b, double *result);
int mulib_opticalmargin_clamp(double input, double lower, double upper, double *result);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_OPTICALMARGIN_H */
#if defined(MULIB_IMPL_OPTICALMARGIN) && !defined(MULIB_OPTICALMARGIN_H_IMPLEMENTED)
#define MULIB_OPTICALMARGIN_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_opticalmargin_compute(double a,double b,double *result){if(!result||!isfinite(a)||!isfinite(b))return 0;double v=(a-b);if(!isfinite(v))return 0;*result=v;return 1;}
int mulib_opticalmargin_clamp(double input,double lower,double upper,double *result){if(!result||!isfinite(input)||!isfinite(lower)||!isfinite(upper)||lower>upper)return 0;*result=input<lower?lower:input>upper?upper:input;return 1;}
#ifdef __cplusplus
}
#endif
#endif
