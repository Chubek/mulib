/* MuLIB browser staging addition: wasm-link
 * Implemented subset: Exact module/name/kind import resolution over supplied exports.
 * Remaining: Type compatibility, instantiation, import cycles and resource lifetime.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_LINK_H
#define MULIB_WASM_LINK_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span module,name;unsigned kind;uintptr_t value;} mulib_wasm_link_export;
static inline int mulib_wasm_link_resolve(const mulib_wasm_link_export *v,size_t n,muba_span module,muba_span name,unsigned kind,uintptr_t *out){if(!out||(!v&&n))return 0;for(size_t i=0;i<n;i++)if(v[i].kind==kind&&muba_eq(v[i].module,module)&&muba_eq(v[i].name,name)){*out=v[i].value;return 1;}return 0;}
#endif /* MULIB_WASM_LINK_H */
