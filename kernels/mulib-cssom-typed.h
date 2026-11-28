/* MuLIB browser staging addition: cssom-typed
 * Implemented subset: Typed numeric value with explicit unit and compatible addition.
 * Remaining: Full CSS Typed OM class hierarchy and math expressions.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSSOM_TYPED_H
#define MULIB_CSSOM_TYPED_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {double value;unsigned unit;} mulib_cssom_typed_number;
static inline int mulib_cssom_typed_add(mulib_cssom_typed_number a,mulib_cssom_typed_number b,mulib_cssom_typed_number *out){if(!out||a.unit!=b.unit||!isfinite(a.value)||!isfinite(b.value)||!isfinite(a.value+b.value))return 0;*out=(mulib_cssom_typed_number){a.value+b.value,a.unit};return 1;}
#endif /* MULIB_CSSOM_TYPED_H */
