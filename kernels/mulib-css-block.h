/* MuLIB browser staging addition: css-block
 * Implemented subset: Adjacent two-margin collapse and checked block vertical extent.
 * Remaining: Formatting contexts, clearance, floats and full margin collapse.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_BLOCK_H
#define MULIB_CSS_BLOCK_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_block_collapse(double a,double b,double *out){if(!out||!isfinite(a)||!isfinite(b))return 0;*out=fmax(0,fmax(a,b))+fmin(0,fmin(a,b));return 1;}
#endif /* MULIB_CSS_BLOCK_H */
