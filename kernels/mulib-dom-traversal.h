/* MuLIB browser staging addition: dom-traversal
 * Implemented subset: Bounded preorder successor via first-child/next-sibling/parent callbacks.
 * Remaining: NodeIterator, TreeWalker filtering and mutation adjustment.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_TRAVERSAL_H
#define MULIB_DOM_TRAVERSAL_H
#include "detail/mulib-browser-additions-common.h"

typedef uint64_t (*mulib_dom_traversal_link)(void *,uint64_t);
static inline uint64_t mulib_dom_traversal_next(uint64_t root,uint64_t node,mulib_dom_traversal_link child,mulib_dom_traversal_link sibling,mulib_dom_traversal_link parent,void *ctx,size_t budget){if(!root||!node||!child||!sibling||!parent)return 0;uint64_t v=child(ctx,node);if(v)return v;while(node&&node!=root&&budget--){v=sibling(ctx,node);if(v)return v;node=parent(ctx,node);}return 0;}
#endif /* MULIB_DOM_TRAVERSAL_H */
