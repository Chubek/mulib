/** @file mulib-doclength.h
 * @brief Convert document lengths among CSS px, PDF pt, inch and mm.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_DOCLENGTH once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-doclength.md for scope and limitations.
 */
#ifndef MULIB_DOCLENGTH_H
#define MULIB_DOCLENGTH_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { mulib_doclength_PT=0,mulib_doclength_PX=1,mulib_doclength_IN=2,mulib_doclength_MM=3 };
int mulib_doclength_convert(double input,unsigned from,unsigned to,double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOCLENGTH_H */
#if defined(MULIB_IMPL_DOCLENGTH) && !defined(MULIB_DOCLENGTH_H_IMPLEMENTED)
#define MULIB_DOCLENGTH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static double mulib_doclength_scale_(unsigned unit){switch(unit){case mulib_doclength_PT:return 1.0;case mulib_doclength_PX:return 0.75;case mulib_doclength_IN:return 72.0;case mulib_doclength_MM:return 72.0/25.4;default:return 0;}}
int mulib_doclength_convert(double input,unsigned from,unsigned to,double *out){double a=mulib_doclength_scale_(from),b=mulib_doclength_scale_(to);if(!out||!isfinite(input)||!a||!b)return 0;double r=input*a/b;if(!isfinite(r))return 0;*out=r;return 1;}
#ifdef __cplusplus
}
#endif
#endif
