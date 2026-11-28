/* MuLIB browser staging addition: wasm-jsbridge
 * Implemented subset: Number to i32 bit-pattern conversion and i32 return conversion.
 * Remaining: BigInt i64, externref, JS objects, promises and module instantiation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_JSBRIDGE_H
#define MULIB_WASM_JSBRIDGE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-webidl-convert.h"
static inline uint32_t mulib_wasm_jsbridge_i32(double x){return mulib_webidl_convert_uint32(x);}
static inline double mulib_wasm_jsbridge_number(uint32_t x){return x<=INT32_MAX?(double)x:(double)((int64_t)x-4294967296LL);}
#endif /* MULIB_WASM_JSBRIDGE_H */
