/* MuLIB browser staging addition: wasm-validate
 * Implemented subset: Wasm v1 MVP section envelope order and uniqueness check.
 * Remaining: Full type/instruction validation, section content and post-MVP order.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_VALIDATE_H
#define MULIB_WASM_VALIDATE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-wasm-decode.h"
static inline int mulib_wasm_validate_mvp_envelopes(muba_span s){muba_cursor c={s,0};if(!mulib_wasm_decode_preamble(&c))return 0;unsigned last=0,id;muba_span b;while(c.pos<s.len){if(!mulib_wasm_decode_section(&c,&id,&b)||id>11||(id&&id<=last))return 0;if(id)last=id;}return 1;}
#endif /* MULIB_WASM_VALIDATE_H */
