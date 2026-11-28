/* MuLIB browser staging addition: wasm-instance
 * Implemented subset: Instance record with explicit host allocation and module identity.
 * Remaining: Actual module instantiation, globals, data segments and start function.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_INSTANCE_H
#define MULIB_WASM_INSTANCE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-wasm-module.h"
typedef struct {const mulib_wasm_module_t *module;uint64_t id;int active;} mulib_wasm_instance_t;
static inline int mulib_wasm_instance_attach(mulib_wasm_instance_t *i,const mulib_wasm_module_t *m,uint64_t id){if(!i||i->active||!m||!id)return 0;*i=(mulib_wasm_instance_t){m,id,1};return 1;}
#endif /* MULIB_WASM_INSTANCE_H */
