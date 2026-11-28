/* MuLIB browser staging addition: dom-collection
 * Implemented subset: Live indexed access through count/item callbacks.
 * Remaining: Named properties, iteration, query filters and version invalidation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_COLLECTION_H
#define MULIB_DOM_COLLECTION_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {void *ctx;size_t (*count)(void *);uint64_t (*item)(void *,size_t);} mulib_dom_collection_t;
static inline uint64_t mulib_dom_collection_at(const mulib_dom_collection_t *c,size_t i){return c&&c->count&&c->item&&i<c->count(c->ctx)?c->item(c->ctx,i):0;}
#endif /* MULIB_DOM_COLLECTION_H */
