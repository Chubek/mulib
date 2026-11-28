/* MuLIB browser staging addition: wasm-module
 * Implemented subset: Borrowed binary section directory with MVP envelope validation.
 * Remaining: Parsed module representation, type checking and ownership.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_MODULE_H
#define MULIB_WASM_MODULE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-wasm-decode.h"
#include "mulib-wasm-validate.h"
typedef struct {muba_span section[12];unsigned present;} mulib_wasm_module_t;
static inline int mulib_wasm_module_read(muba_span s,mulib_wasm_module_t *out){if(!out||!mulib_wasm_validate_mvp_envelopes(s))return 0;mulib_wasm_module_t m={0};muba_cursor c={s,8};while(c.pos<s.len){unsigned id;muba_span b;if(!mulib_wasm_decode_section(&c,&id,&b))return 0;if(id){m.section[id]=b;m.present|=1u<<id;}}*out=m;return 1;}
#endif /* MULIB_WASM_MODULE_H */
