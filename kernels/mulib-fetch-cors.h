/* MuLIB browser staging addition: fetch-cors
 * Implemented subset: Access-Control-Allow-Origin and credentials check with exact serialized origins.
 * Remaining: Preflight, exposed headers, safelist, opaque origins and Fetch integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_CORS_H
#define MULIB_FETCH_CORS_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_fetch_cors_allow(muba_span origin,muba_span acao,int credentials,muba_span acac){if(!muba_span_ok(origin)||!origin.len)return 0;acao=muba_trim(acao);if(credentials&&!muba_eq(muba_trim(acac),muba_str("true")))return 0;if(!credentials&&muba_eq(acao,muba_str("*")))return 1;return muba_eq(origin,acao);}
#endif /* MULIB_FETCH_CORS_H */
