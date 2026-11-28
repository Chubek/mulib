/* MuLIB browser staging addition: microjs-coercion
 * Implemented subset: Primitive truthiness, strict numeric equality and ToInt32 modulo.
 * Remaining: Object coercion, string-to-number, abstract equality and engine value adapter.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_COERCION_H
#define MULIB_MICROJS_COERCION_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_microjs_coercion_number_truth(double n){return n!=0&&!isnan(n);}
static inline int mulib_microjs_coercion_number_equal(double a,double b){return a==b;}
static inline int32_t mulib_microjs_coercion_int32(double d){if(!isfinite(d)||d==0)return 0;double x=fmod(trunc(d),4294967296.0);if(x<0)x+=4294967296.0;if(x>=2147483648.0)x-=4294967296.0;return (int32_t)x;}
#endif /* MULIB_MICROJS_COERCION_H */
