/* MuLIB browser staging addition: web-locks
 * Implemented subset: Bounded exclusive/shared lock compatibility check.
 * Remaining: Wait queues, lock manager, abort and JS callback completion.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_LOCKS_H
#define MULIB_WEB_LOCKS_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span name;uint64_t owner;int exclusive;} mulib_web_locks_held;

/* An incomplete held-lock snapshot cannot establish availability. */
static inline int mulib_web_locks_available(const mulib_web_locks_held *v,
                                           size_t n, muba_span name,
                                           int exclusive)
{
    if ((!v && n) || !muba_span_ok(name))
        return 0;
    for (size_t i = 0; i < n; i++) {
        if (!muba_span_ok(v[i].name) ||
            (muba_eq(v[i].name, name) && (exclusive || v[i].exclusive)))
            return 0;
    }
    return 1;
}
#endif /* MULIB_WEB_LOCKS_H */
