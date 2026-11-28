/* MuLIB browser staging addition: browser-diff
 * Implemented subset: Emit changed, added and removed stable-ID/version records between snapshots.
 * Remaining: Tree ordering, payloads and incremental reconciliation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_DIFF_H
#define MULIB_BROWSER_DIFF_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id,version;} mulib_browser_diff_entry;
typedef int (*mulib_browser_diff_emit_fn)(void *,unsigned,uint64_t);
/* Both snapshots must have nonzero, unique IDs within each snapshot.
 * Entries may be unsorted and must remain unchanged throughout the call.
 * Invalid snapshots are rejected before the first callback. For valid input,
 * emit changes (2) and removals (3) in old order, then additions (1) in now
 * order. Callback rejection stops immediately; earlier effects are retained.
 * No allocations; validation and matching are quadratic in the entry counts.
 */
static inline int
mulib_browser_diff_run(const mulib_browser_diff_entry *old, size_t a,
                       const mulib_browser_diff_entry *now, size_t b,
                       mulib_browser_diff_emit_fn emit, void *ctx)
{
    if (!emit || (!old && a) || (!now && b))
        return 0;
    for (size_t i = 0; i < a; ++i) {
        if (!old[i].id)
            return 0;
        for (size_t j = 0; j < i; ++j)
            if (old[j].id == old[i].id)
                return 0;
    }
    for (size_t i = 0; i < b; ++i) {
        if (!now[i].id)
            return 0;
        for (size_t j = 0; j < i; ++j)
            if (now[j].id == now[i].id)
                return 0;
    }
    for (size_t i = 0; i < a; ++i) {
        size_t j = 0;
        while (j < b && now[j].id != old[i].id)
            ++j;
        if (j == b) {
            if (!emit(ctx, 3, old[i].id))
                return 0;
        } else if (now[j].version != old[i].version
                   && !emit(ctx, 2, old[i].id))
            return 0;
    }
    for (size_t j = 0; j < b; ++j) {
        size_t i = 0;
        while (i < a && old[i].id != now[j].id)
            ++i;
        if (i == a && !emit(ctx, 1, now[j].id))
            return 0;
    }
    return 1;
}
#endif /* MULIB_BROWSER_DIFF_H */
