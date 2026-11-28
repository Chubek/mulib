/* MuLIB browser staging addition: dom-observer
 * Implemented subset: Bounded mutation record queue with synchronous batch drain.
 * Remaining: MutationObserver registration, filtering, microtask delivery and DOM integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_OBSERVER_H
#define MULIB_DOM_OBSERVER_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t target,related;unsigned kind;} mulib_dom_observer_record;
typedef struct {mulib_dom_observer_record *v;size_t len,cap;int delivering;} mulib_dom_observer_t;
static inline int mulib_dom_observer_push(mulib_dom_observer_t *q,mulib_dom_observer_record r){if(!q||!r.target||q->len>=q->cap)return 0;q->v[q->len++]=r;return 1;}
static inline size_t mulib_dom_observer_take(mulib_dom_observer_t *q,mulib_dom_observer_record *out,size_t cap){if(!q||(!out&&cap))return 0;size_t n=q->len<cap?q->len:cap;if(n){memcpy(out,q->v,n*sizeof *out);memmove(q->v,q->v+n,(q->len-n)*sizeof *out);q->len-=n;}return n;}
#endif /* MULIB_DOM_OBSERVER_H */
