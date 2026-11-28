/* MuLIB browser staging addition: browser-observer
 * Implemented subset: Threshold crossing detection for intersection-style ratios.
 * Remaining: Resize/IntersectionObserver geometry, registration and callback scheduling.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_OBSERVER_H
#define MULIB_BROWSER_OBSERVER_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_browser_observer_crossed(double before,double after,double threshold){return isfinite(before)&&isfinite(after)&&isfinite(threshold)&&threshold>=0&&threshold<=1&&before>=0&&before<=1&&after>=0&&after<=1&&((before<threshold)!=(after<threshold));}
#endif /* MULIB_BROWSER_OBSERVER_H */
