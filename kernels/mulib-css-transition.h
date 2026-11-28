/* MuLIB browser staging addition: css-transition
 * Implemented subset: Linear interpolation of finite numeric endpoints.
 * Remaining: Property interpolation, timing functions, reversal and transition events.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_TRANSITION_H
#define MULIB_CSS_TRANSITION_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_transition_sample(double a,double b,double progress,double *out){if(!out||!isfinite(a)||!isfinite(b)||!isfinite(progress))return 0;double t=fmax(0,fmin(1,progress));double v=a*(1-t)+b*t;if(!isfinite(v))return 0;*out=v;return 1;}
#endif /* MULIB_CSS_TRANSITION_H */
