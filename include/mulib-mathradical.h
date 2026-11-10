/** @file mulib-mathradical.h
 * @brief Calculate radical clearance and total ascent.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_MATHRADICAL once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-mathradical.md for scope and limitations.
 */
#ifndef MULIB_MATHRADICAL_H
#define MULIB_MATHRADICAL_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathradical_ascent(double content_ascent,double rule,double clearance,double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATHRADICAL_H */
#if defined(MULIB_IMPL_MATHRADICAL) && !defined(MULIB_MATHRADICAL_H_IMPLEMENTED)
#define MULIB_MATHRADICAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathradical_ascent(double content,double rule,double gap,double *out){if(!out||!isfinite(content)||!isfinite(rule)||!isfinite(gap)||content<0||rule<0||gap<0)return 0;*out=content+rule+gap;return isfinite(*out);}
#ifdef __cplusplus
}
#endif
#endif
