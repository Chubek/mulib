/* MuLIB browser staging addition: dom-tokenlist
 * Implemented subset: ASCII whitespace token containment and validation.
 * Remaining: Live reflected token list, mutation and exceptions.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_TOKENLIST_H
#define MULIB_DOM_TOKENLIST_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_dom_tokenlist_valid(muba_span t){if(!muba_span_ok(t)||!t.len)return 0;for(size_t i=0;i<t.len;i++)if(muba_space(t.data[i]))return 0;return 1;}
static inline int mulib_dom_tokenlist_contains(muba_span s,muba_span t){if(!muba_span_ok(s)||!mulib_dom_tokenlist_valid(t))return 0;for(size_t i=0;i<s.len;){while(i<s.len&&muba_space(s.data[i]))i++;size_t j=i;while(i<s.len&&!muba_space(s.data[i]))i++;if(i>j&&muba_eq((muba_span){s.data+j,i-j},t))return 1;}return 0;}
#endif /* MULIB_DOM_TOKENLIST_H */
