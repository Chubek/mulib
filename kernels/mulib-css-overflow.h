/* MuLIB browser staging addition: css-overflow
 * Implemented subset: Scroll range and clamped scroll position for one axis.
 * Remaining: Overflow propagation, writing modes, scroll events and smooth scroll.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_OVERFLOW_H
#define MULIB_CSS_OVERFLOW_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_overflow_scroll(double viewport,double content,double requested,double *out){if(!out||!isfinite(viewport)||!isfinite(content)||!isfinite(requested)||viewport<0||content<0)return 0;*out=fmin(fmax(0,requested),fmax(0,content-viewport));return 1;}
#endif /* MULIB_CSS_OVERFLOW_H */
