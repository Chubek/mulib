/* MuLIB browser staging addition: web-reporting
 * Implemented subset: Bounded report queue with origin/type and timestamp.
 * Remaining: Reporting endpoints, delivery, privacy and retries.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_REPORTING_H
#define MULIB_WEB_REPORTING_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t origin,time;unsigned type;uintptr_t body;} mulib_web_reporting_report;
static inline int mulib_web_reporting_push(mulib_web_reporting_report *v,size_t *n,size_t cap,mulib_web_reporting_report r){if(!v||!n||*n>=cap||!r.origin)return 0;v[(*n)++]=r;return 1;}
#endif /* MULIB_WEB_REPORTING_H */
