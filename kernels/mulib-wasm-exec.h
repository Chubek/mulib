/* MuLIB browser staging addition: wasm-exec
 * Implemented subset: MVP i32 arithmetic helpers with divide-by-zero and overflow traps.
 * Remaining: Instruction interpreter, control stack, calls, memory and validation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_EXEC_H
#define MULIB_WASM_EXEC_H
#include "detail/mulib-browser-additions-common.h"

static inline uint32_t mulib_wasm_exec_add(uint32_t a,uint32_t b){return a+b;}
static inline uint32_t mulib_wasm_exec_mul(uint32_t a,uint32_t b){return a*b;}
static inline int mulib_wasm_exec_div_s(int32_t a,int32_t b,int32_t *out){if(!out||!b||(a==INT32_MIN&&b==-1))return 0;*out=a/b;return 1;}
#endif /* MULIB_WASM_EXEC_H */
