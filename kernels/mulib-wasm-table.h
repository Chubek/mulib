/* MuLIB browser staging addition: wasm-table
 * Implemented subset: Caller-owned reference table get/set with bounds.
 * Remaining: Growth, type checking, function references and GC.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_TABLE_H
#define MULIB_WASM_TABLE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uintptr_t *v;size_t n;} mulib_wasm_table_t;
static inline int mulib_wasm_table_get(const mulib_wasm_table_t *t,size_t i,uintptr_t *out){if(!t||!out||i>=t->n)return 0;*out=t->v[i];return 1;}
static inline int mulib_wasm_table_set(mulib_wasm_table_t *t,size_t i,uintptr_t v){if(!t||i>=t->n)return 0;t->v[i]=v;return 1;}
#endif /* MULIB_WASM_TABLE_H */
