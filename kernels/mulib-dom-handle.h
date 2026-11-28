/* MuLIB browser staging addition: dom-handle
 * Implemented subset: Generation-checked bounded native handles with release.
 * Remaining: Tracing, weak handles, concurrency and ownership integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_HANDLE_H
#define MULIB_DOM_HANDLE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uintptr_t object;uint32_t generation;} mulib_dom_handle_slot;
typedef struct {mulib_dom_handle_slot *slots;size_t cap;} mulib_dom_handle_table;
static inline uint64_t mulib_dom_handle_insert(mulib_dom_handle_table *t,uintptr_t v){if(!t||!v||t->cap>UINT32_MAX)return 0;for(size_t i=0;i<t->cap;i++){ mulib_dom_handle_slot *s=t->slots+i;if(!s->object&&s->generation<UINT32_MAX){s->object=v;s->generation++;return ((uint64_t)s->generation<<32)|(uint32_t)(i+1);}}return 0;}
static inline uintptr_t mulib_dom_handle_get(const mulib_dom_handle_table *t,uint64_t h){uint32_t i=(uint32_t)h;if(!t||!i||i>t->cap)return 0;const mulib_dom_handle_slot *s=t->slots+i-1;return s->generation==(uint32_t)(h>>32)?s->object:0;}
static inline int mulib_dom_handle_remove(mulib_dom_handle_table *t,uint64_t h){if(!mulib_dom_handle_get(t,h))return 0;t->slots[(uint32_t)h-1].object=0;return 1;}
#endif /* MULIB_DOM_HANDLE_H */
