/** @file mulib-colormodel.h
 * @brief Blend numeric color-channel values using supplied opacity. Finite numeric inputs and documented scalar result.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_COLORMODEL once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-colormodel.md for scope and limitations.
 */
#ifndef MULIB_COLORMODEL_H
#define MULIB_COLORMODEL_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_colormodel_compute(double a, double b, double *result);
int mulib_colormodel_clamp(double input, double lower, double upper, double *result);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_COLORMODEL_H */
#if defined(MULIB_IMPL_COLORMODEL) && !defined(MULIB_COLORMODEL_H_IMPLEMENTED)
#define MULIB_COLORMODEL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_colormodel_compute(double a,double b,double *result){if(!result||!isfinite(a)||!isfinite(b))return 0;double v=(a*b);if(!isfinite(v))return 0;*result=v;return 1;}
int mulib_colormodel_clamp(double input,double lower,double upper,double *result){if(!result||!isfinite(input)||!isfinite(lower)||!isfinite(upper)||lower>upper)return 0;*result=input<lower?lower:input>upper?upper:input;return 1;}
#ifdef __cplusplus
}
#endif
#endif
