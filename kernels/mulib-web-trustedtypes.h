/* MuLIB browser staging addition: web-trustedtypes
 * Implemented subset: Brand and policy identity check for host-created trusted values.
 * Remaining: Unforgeable JS platform objects, policy registry and CSP sink enforcement.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_TRUSTEDTYPES_H
#define MULIB_WEB_TRUSTEDTYPES_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t policy;unsigned kind;muba_span value;} mulib_web_trustedtypes_t;
static inline int mulib_web_trustedtypes_matches(const mulib_web_trustedtypes_t *v,uint64_t policy,unsigned kind){return v&&policy&&v->policy==policy&&v->kind==kind&&muba_span_ok(v->value);}
#endif /* MULIB_WEB_TRUSTEDTYPES_H */
