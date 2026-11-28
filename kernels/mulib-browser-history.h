/* MuLIB browser staging addition: browser-history
 * Implemented subset: Bounded session history push and signed traversal.
 * Remaining: Replace, joint history, navigation, BFCache and serialized state.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_HISTORY_H
#define MULIB_BROWSER_HISTORY_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t document;uintptr_t state;muba_span url;} mulib_browser_history_entry;
typedef struct {mulib_browser_history_entry *v;size_t len,cap,current;} mulib_browser_history_t;
static inline int mulib_browser_history_push(mulib_browser_history_t *h,mulib_browser_history_entry e){if(!h||!e.document||!muba_span_ok(e.url)||(h->len&&h->current>=h->len))return 0;size_t i=h->len?h->current+1:0;if(i>=h->cap)return 0;h->v[i]=e;h->len=i+1;h->current=i;return 1;}
static inline int mulib_browser_history_go(mulib_browser_history_t *h,int64_t delta){if(!h||!h->len||h->current>=h->len)return 0;uint64_t mag=delta<0?(uint64_t)(-(delta+1))+1:(uint64_t)delta;if(delta<0){if(mag>h->current)return 0;h->current-=(size_t)mag;}else{if(mag>=h->len-h->current)return 0;h->current+=(size_t)mag;}return 1;}
#endif /* MULIB_BROWSER_HISTORY_H */
