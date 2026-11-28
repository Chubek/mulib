/* MuLIB browser staging addition: css-position
 * Implemented subset: Absolute horizontal/vertical position from start or end inset.
 * Remaining: Containing blocks, relative/sticky positioning and sizing constraints.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_POSITION_H
#define MULIB_CSS_POSITION_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_position_axis(double container,double size,double start,double end,double *out){if(!out||!isfinite(container)||!isfinite(size)||container<0||size<0)return 0;double x=isfinite(start)?start:isfinite(end)?container-size-end:0;if(!isfinite(x))return 0;*out=x;return 1;}
#endif /* MULIB_CSS_POSITION_H */
