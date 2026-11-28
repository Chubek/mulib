/* MuLIB browser staging addition: microjs-bigint
 * Implemented subset: BigInt literal parsing and checked arithmetic on bounded bigint.
 * Remaining: Full BigInt operators, arbitrary precision, coercion and VM adapter.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_BIGINT_H
#define MULIB_MICROJS_BIGINT_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-bigint.h"
static inline int mulib_microjs_bigint_literal(muba_span s,mulib_bigint_t *out){if(!muba_span_ok(s)||s.len<2||s.data[s.len-1]!='n')return 0;s.len--;return mulib_bigint_parse(s,out);}
static inline int mulib_microjs_bigint_add(const mulib_bigint_t *a,const mulib_bigint_t *b,mulib_bigint_t *out){return mulib_bigint_add(a,b,out);}
#endif /* MULIB_MICROJS_BIGINT_H */
