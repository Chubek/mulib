/* MuLIB browser staging addition: dom-slot
 * Implemented subset: First matching named-slot lookup over borrowed name records.
 * Remaining: Automatic/manual assignment, slotchange and flattened tree.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_SLOT_H
#define MULIB_DOM_SLOT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id;muba_span name;} mulib_dom_slot_entry;
static inline uint64_t mulib_dom_slot_find(const mulib_dom_slot_entry *v,size_t n,muba_span name){if(v)for(size_t i=0;i<n;i++)if(muba_eq(v[i].name,name))return v[i].id;return 0;}
#endif /* MULIB_DOM_SLOT_H */
