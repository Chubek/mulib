/* MuLIB browser staging addition: css-intrinsic
 * Implemented subset: Fit-content size from min-content, max-content and available size.
 * Remaining: Actual intrinsic measurement, replaced elements and percentage rules.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_INTRINSIC_H
#define MULIB_CSS_INTRINSIC_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_intrinsic_fit(double min,double max,double avail,double *out){if(!out||!isfinite(min)||!isfinite(max)||!isfinite(avail)||min<0||max<min)return 0;*out=fmin(max,fmax(min,avail));return 1;}
#endif /* MULIB_CSS_INTRINSIC_H */
