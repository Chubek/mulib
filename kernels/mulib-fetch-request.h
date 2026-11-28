/* MuLIB browser staging addition: fetch-request
 * Implemented subset: HTTP method token validation and forbidden method check.
 * Remaining: Request construction, modes, credentials, cloning and bodies.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_REQUEST_H
#define MULIB_FETCH_REQUEST_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_fetch_request_method(muba_span s){if(!muba_span_ok(s)||!s.len)return 0;for(size_t i=0;i<s.len;i++){unsigned char c=s.data[i];if(!c||(!muba_alpha(c)&&!muba_digit(c)&&!strchr("!#$%&'*+-.^_`|~",c)))return 0;}return !muba_ieq(s,muba_str("CONNECT"))&&!muba_ieq(s,muba_str("TRACE"))&&!muba_ieq(s,muba_str("TRACK"));}
#endif /* MULIB_FETCH_REQUEST_H */
