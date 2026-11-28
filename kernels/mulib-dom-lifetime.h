/* MuLIB browser staging addition: dom-lifetime
 * Implemented subset: Explicit retain/release with checked reference count and finalizer callback.
 * Remaining: Cycle tracing across JS and DOM, document ownership and GC integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_LIFETIME_H
#define MULIB_DOM_LIFETIME_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {size_t refs;void (*destroy)(void *);void *ctx;} mulib_dom_lifetime_t;
static inline int mulib_dom_lifetime_retain(mulib_dom_lifetime_t *r){if(!r||!r->refs||r->refs==SIZE_MAX)return 0;r->refs++;return 1;}
static inline int mulib_dom_lifetime_release(mulib_dom_lifetime_t *r){if(!r||!r->refs)return 0;if(--r->refs==0&&r->destroy)r->destroy(r->ctx);return 1;}
#endif /* MULIB_DOM_LIFETIME_H */
