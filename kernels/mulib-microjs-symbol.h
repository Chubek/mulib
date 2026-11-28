/* MuLIB browser staging addition: microjs-symbol
 * Implemented subset: Monotonic symbol identifiers and bounded registry with borrowed keys.
 * Remaining: Well-known symbols, GC, per-agent registry and VM values.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_SYMBOL_H
#define MULIB_MICROJS_SYMBOL_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span key;uint64_t id;} mulib_microjs_symbol_entry;
typedef struct {mulib_microjs_symbol_entry *entries;size_t len,cap;uint64_t next;} mulib_microjs_symbol_registry;
static inline int mulib_microjs_symbol_new(mulib_microjs_symbol_registry *r,uint64_t *id){if(!r||!id||r->next==UINT64_MAX)return 0;*id=++r->next;return 1;}
static inline int mulib_microjs_symbol_for(mulib_microjs_symbol_registry *r,muba_span key,uint64_t *id){if(!r||!id||!muba_span_ok(key))return 0;for(size_t i=0;i<r->len;i++)if(muba_eq(r->entries[i].key,key)){*id=r->entries[i].id;return 1;}if(r->len>=r->cap||!mulib_microjs_symbol_new(r,id))return 0;r->entries[r->len++]=(mulib_microjs_symbol_entry){key,*id};return 1;}
#endif /* MULIB_MICROJS_SYMBOL_H */
