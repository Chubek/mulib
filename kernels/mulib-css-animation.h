/* MuLIB browser staging addition: css-animation
 * Implemented subset: Linear normalized time progress with iteration count.
 * Remaining: Timing functions, keyframes, fill/direction, compositing and timeline.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_ANIMATION_H
#define MULIB_CSS_ANIMATION_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_animation_progress(double elapsed,double duration,double iterations,double *out){if(!out||!isfinite(elapsed)||!isfinite(duration)||!isfinite(iterations)||duration<=0||iterations<=0)return 0;if(elapsed<=0){*out=0;return 1;}double t=elapsed/duration;if(t>=iterations){double f=iterations-floor(iterations);*out=f?f:1;}else *out=t-floor(t);return 1;}
#endif /* MULIB_CSS_ANIMATION_H */
