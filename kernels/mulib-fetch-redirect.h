/* MuLIB browser staging addition: fetch-redirect
 * Implemented subset: Redirect status detection and method rewrite predicate.
 * Remaining: URL resolution, origin changes, body replay, redirect limits and referrer updates.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_REDIRECT_H
#define MULIB_FETCH_REDIRECT_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_fetch_redirect_status(unsigned s){return s==301||s==302||s==303||s==307||s==308;}
static inline int mulib_fetch_redirect_rewrite_get(unsigned s,muba_span method){return ((s==301||s==302)&&muba_eq(method,muba_str("POST")))||(s==303&&!muba_eq(method,muba_str("GET"))&&!muba_eq(method,muba_str("HEAD")));}
#endif /* MULIB_FETCH_REDIRECT_H */
