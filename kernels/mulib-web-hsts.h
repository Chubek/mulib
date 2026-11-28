/* MuLIB browser staging addition: web-hsts
 * Implemented subset: Checked max-age expiry calculation with saturation.
 * Remaining: Header parsing, includeSubDomains, preload and transport integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_HSTS_H
#define MULIB_WEB_HSTS_H
#include "detail/mulib-browser-additions-common.h"

static inline uint64_t mulib_web_hsts_expiry(uint64_t now,uint64_t max_age){return max_age>UINT64_MAX-now?UINT64_MAX:now+max_age;}
static inline int mulib_web_hsts_active(uint64_t expiry,uint64_t now){return expiry>now;}
#endif /* MULIB_WEB_HSTS_H */
