/* MuLIB browser staging addition: css-supports
 * Implemented subset: Feature query dispatch through explicit declaration parser callback.
 * Remaining: Supports condition grammar, selector queries and declaration parsing.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_SUPPORTS_H
#define MULIB_CSS_SUPPORTS_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_css_supports_test)(void *,muba_span,muba_span);
static inline int mulib_css_supports_declaration(mulib_css_supports_test test,void *ctx,muba_span property,muba_span value){return test&&property.len&&muba_span_ok(property)&&muba_span_ok(value)&&test(ctx,property,value)>0;}
#endif /* MULIB_CSS_SUPPORTS_H */
