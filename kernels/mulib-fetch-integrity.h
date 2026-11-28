/* MuLIB browser staging addition: fetch-integrity
 * Implemented subset: Constant-time digest equality for host-computed digest.
 * Remaining: SRI parsing, hash selection, CORS eligibility and cryptographic provider.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_INTEGRITY_H
#define MULIB_FETCH_INTEGRITY_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_fetch_integrity_equal(muba_span expected,muba_span computed){if(!muba_span_ok(expected)||!muba_span_ok(computed)||!expected.len||expected.len!=computed.len)return 0;unsigned diff=0;for(size_t i=0;i<expected.len;i++)diff|=expected.data[i]^computed.data[i];return diff==0;}
#endif /* MULIB_FETCH_INTEGRITY_H */
