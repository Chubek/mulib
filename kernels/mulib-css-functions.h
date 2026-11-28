/* MuLIB browser staging addition: css-functions
 * Implemented subset: Numeric min/max/clamp and checked same-unit arithmetic.
 * Remaining: CSS math expression parsing, type algebra and deferred resolution.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_FUNCTIONS_H
#define MULIB_CSS_FUNCTIONS_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_functions_clamp(double lo,double v,double hi,double *out){if(!out||!isfinite(lo)||!isfinite(v)||!isfinite(hi))return 0;*out=fmax(lo,fmin(v,hi));return 1;}
static inline int mulib_css_functions_binary(char op,double a,double b,double *out){if(!out||!isfinite(a)||!isfinite(b))return 0;double v;switch(op){case '+':v=a+b;break;case '-':v=a-b;break;case '*':v=a*b;break;case '/':if(!b)return 0;v=a/b;break;default:return 0;}if(!isfinite(v))return 0;*out=v;return 1;}
#endif /* MULIB_CSS_FUNCTIONS_H */
