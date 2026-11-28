/* MuLIB browser staging addition: fetch-response
 * Implemented subset: Status validation and null-body status classification.
 * Remaining: Response filtering, opaque redirects and body lifetime.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_RESPONSE_H
#define MULIB_FETCH_RESPONSE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_fetch_response_ok(unsigned status){return status>=200&&status<=299;}
static inline int mulib_fetch_response_null_body(unsigned status){return status==101||status==103||status==204||status==205||status==304;}
#endif /* MULIB_FETCH_RESPONSE_H */
