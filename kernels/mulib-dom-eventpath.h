/* MuLIB browser staging addition: dom-eventpath
 * Implemented subset: Build bounded ancestor path via host parent callback, reject cycles.
 * Remaining: Shadow retargeting, composed paths and relatedTarget semantics.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_EVENTPATH_H
#define MULIB_DOM_EVENTPATH_H
#include "detail/mulib-browser-additions-common.h"

typedef uint64_t (*mulib_dom_eventpath_parent_fn)(void *,uint64_t);
static inline int mulib_dom_eventpath_build(uint64_t target,mulib_dom_eventpath_parent_fn parent,void *ctx,uint64_t *v,size_t cap,size_t *n){if(!target||!parent||!v||!n)return 0;size_t i=0;while(target){if(i==cap)return 0;for(size_t j=0;j<i;j++)if(v[j]==target)return 0;v[i++]=target;target=parent(ctx,target);}*n=i;return 1;}
#endif /* MULIB_DOM_EVENTPATH_H */
