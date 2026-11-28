/* MuLIB browser staging addition: web-worklet
 * Implemented subset: Bounded dispatch by unique registered processor name.
 * Remaining: Worklet modules, isolated realms, audio/paint scheduling and deadlines.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WORKLET_H
#define MULIB_WEB_WORKLET_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_worklet_fn)(void *,uintptr_t);
typedef struct {muba_span name;mulib_web_worklet_fn fn;void *ctx;} mulib_web_worklet_entry;

/* Resolve before invoking any callback; ambiguous names do not dispatch. */
static inline int mulib_web_worklet_run(const mulib_web_worklet_entry *v,
                                      size_t n, muba_span name, uintptr_t arg)
{
    if (!v || !name.data || !name.len)
        return 0;

    const mulib_web_worklet_entry *match = NULL;
    for (size_t i = 0; i < n; i++) {
        if (!muba_eq(v[i].name, name))
            continue;
        if (match || !v[i].fn)
            return 0;
        match = &v[i];
    }
    return match ? match->fn(match->ctx, arg) : 0;
}
#endif /* MULIB_WEB_WORKLET_H */
