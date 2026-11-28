/* MuLIB browser staging addition: wasm-types
 * Implemented subset: MVP numeric value-type decoding and size lookup.
 * Remaining: Reference, function, GC, SIMD and recursive types.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_TYPES_H
#define MULIB_WASM_TYPES_H
#include "detail/mulib-browser-additions-common.h"

static inline size_t mulib_wasm_types_size(unsigned type){switch(type){case 0x7f:case 0x7d:return 4;case 0x7e:case 0x7c:return 8;default:return 0;}}
#endif /* MULIB_WASM_TYPES_H */
