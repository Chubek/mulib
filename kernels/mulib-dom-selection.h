/* MuLIB browser staging addition: dom-selection
 * Implemented subset: Selection anchor/focus offsets and collapsed predicate.
 * Remaining: Live Range adjustment, direction, tree validation and events.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_SELECTION_H
#define MULIB_DOM_SELECTION_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t anchor,focus;size_t anchor_offset,focus_offset;} mulib_dom_selection_t;
static inline int mulib_dom_selection_collapsed(const mulib_dom_selection_t *s){return s&&s->anchor==s->focus&&s->anchor_offset==s->focus_offset;}
static inline int mulib_dom_selection_collapse(mulib_dom_selection_t *s,uint64_t node,size_t offset){if(!s||!node)return 0;*s=(mulib_dom_selection_t){node,node,offset,offset};return 1;}
#endif /* MULIB_DOM_SELECTION_H */
