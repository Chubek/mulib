/** @file mulib-mathscript.h
 * @brief Scripted baseline offset calculations.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_MATHSCRIPT once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-mathscript.md for scope and limitations.
 */
#ifndef MULIB_MATHSCRIPT_H
#define MULIB_MATHSCRIPT_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathscript_offset(double base_ascent,double script_ascent,double gap,double *upshift);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATHSCRIPT_H */
#if defined(MULIB_IMPL_MATHSCRIPT) && !defined(MULIB_MATHSCRIPT_H_IMPLEMENTED)
#define MULIB_MATHSCRIPT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathscript_offset(double base,double script,double gap,double *out){if(!out||!isfinite(base)||!isfinite(script)||!isfinite(gap)||base<0||script<0||gap<0)return 0;*out=fmax(base,script)+gap;return isfinite(*out);}
#ifdef __cplusplus
}
#endif
#endif
