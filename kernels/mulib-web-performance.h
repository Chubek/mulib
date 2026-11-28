/* MuLIB browser staging addition: web-performance
 * Implemented subset: Caller-clocked monotonic-relative time and duration validation.
 * Remaining: Performance entries, observer delivery and timing privacy.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_PERFORMANCE_H
#define MULIB_WEB_PERFORMANCE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_performance_now(uint64_t origin,uint64_t now,double *ms){if(!ms||now<origin)return 0;*ms=(double)(now-origin)/1000000.0;return 1;}
#endif /* MULIB_WEB_PERFORMANCE_H */
