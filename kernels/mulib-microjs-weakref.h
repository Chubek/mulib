/* MuLIB browser staging addition: microjs-weakref
 * Implemented subset: Weak handle clearing against host liveness predicate.
 * Remaining: GC ephemerons, WeakMap/Set, finalization jobs and rooting.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_WEAKREF_H
#define MULIB_MICROJS_WEAKREF_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_microjs_weakref_alive_fn)(void *,uintptr_t);
typedef struct {uintptr_t target;} mulib_microjs_weakref_t;
static inline uintptr_t mulib_microjs_weakref_sweep(mulib_microjs_weakref_t *w,mulib_microjs_weakref_alive_fn alive,void *ctx){if(!w||!alive)return 0;if(w->target&&!alive(ctx,w->target))w->target=0;return w->target;}
#endif /* MULIB_MICROJS_WEAKREF_H */
