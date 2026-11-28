/* MuLIB browser staging addition: dom-implementation
 * Implemented subset: Document factory dispatch by document kind.
 * Remaining: Actual document creation, DOMParser, XML/HTML integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_IMPLEMENTATION_H
#define MULIB_DOM_IMPLEMENTATION_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_dom_implementation_XML,mulib_dom_implementation_HTML} mulib_dom_implementation_kind;
typedef uintptr_t (*mulib_dom_implementation_factory)(void *,mulib_dom_implementation_kind,muba_span);
static inline uintptr_t mulib_dom_implementation_create(mulib_dom_implementation_factory f,void *ctx,mulib_dom_implementation_kind k,muba_span title){return f&&muba_span_ok(title)&&(k==mulib_dom_implementation_XML||k==mulib_dom_implementation_HTML)?f(ctx,k,title):0;}
#endif /* MULIB_DOM_IMPLEMENTATION_H */
