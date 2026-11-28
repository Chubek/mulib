/* MuLIB browser staging addition: microjs-atomics
 * Implemented subset: C11 sequentially consistent int32 load/store/add/compare exchange.
 * Remaining: Wait/notify, shared buffers, JS conversion and agent memory rules.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_ATOMICS_H
#define MULIB_MICROJS_ATOMICS_H
#include "detail/mulib-browser-additions-common.h"

#include <stdatomic.h>
static inline int32_t mulib_microjs_atomics_load(const _Atomic int32_t *p){return atomic_load(p);}
static inline void mulib_microjs_atomics_store(_Atomic int32_t *p,int32_t v){atomic_store(p,v);}
static inline int32_t mulib_microjs_atomics_add(_Atomic int32_t *p,int32_t v){return atomic_fetch_add(p,v);}
static inline int32_t mulib_microjs_atomics_compare_exchange(_Atomic int32_t *p,int32_t old,int32_t v){atomic_compare_exchange_strong(p,&old,v);return old;}
#endif /* MULIB_MICROJS_ATOMICS_H */
