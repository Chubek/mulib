/* MuLIB browser staging addition: browser-replay
 * Implemented subset: Ordered replay with optional contiguous-history validation.
 * Remaining: Capture nondeterministic inputs, timing, deterministic execution and persistence.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_REPLAY_H
#define MULIB_BROWSER_REPLAY_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-browser-journal.h"
typedef int (*mulib_browser_replay_emit_fn)(void *,const mulib_browser_journal_event *);
static inline int mulib_browser_replay_run(const mulib_browser_journal_t *j,uint64_t *after,mulib_browser_replay_emit_fn emit,void *ctx){if(!j||!after||!emit)return 0;for(size_t i=0;i<j->len;i++)if(j->v[i].seq>*after){if(!emit(ctx,j->v+i))return 0;*after=j->v[i].seq;}return 1;}

typedef enum {
    MULIB_BROWSER_REPLAY_INVALID = 0,
    MULIB_BROWSER_REPLAY_COMPLETE,
    MULIB_BROWSER_REPLAY_GAP,
    MULIB_BROWSER_REPLAY_REJECTED
} mulib_browser_replay_result;

/* Use a cursor for this journal's sequence space. A GAP means the cursor
 * predates retained history, including after clear; install a fresh
 * authoritative snapshot with its matching sequence before retrying.
 * INVALID and GAP leave the cursor unchanged and invoke no callbacks.
 * REJECTED leaves the cursor at the last accepted event for retry.
 * Events and payloads are borrowed during the callback. The callback must
 * not mutate the journal or cursor. External synchronization is required.
 */
static inline mulib_browser_replay_result
mulib_browser_replay_run_contiguous(const mulib_browser_journal_t *j,
                                   uint64_t *after,
                                   mulib_browser_replay_emit_fn emit, void *ctx)
{
    if (!j || !after || !emit || *after > j->seq
        || j->len > j->cap || (j->len && !j->v))
        return MULIB_BROWSER_REPLAY_INVALID;

    /* Append assigns consecutive sequence numbers; clear drops the entire
     * retained prefix without resetting seq. Avoid cursor+1 at UINT64_MAX. */
    uint64_t pending = j->seq - *after;
    if (pending > j->len)
        return MULIB_BROWSER_REPLAY_GAP;
    size_t start = j->len - (size_t)pending;
    for (size_t i = start; i < j->len; ++i)
        if (j->v[i].seq != j->seq - (j->len - i - 1))
            return MULIB_BROWSER_REPLAY_GAP;
    for (size_t i = start; i < j->len; ++i) {
        if (!emit(ctx, j->v + i))
            return MULIB_BROWSER_REPLAY_REJECTED;
        *after = j->v[i].seq;
    }
    return MULIB_BROWSER_REPLAY_COMPLETE;
}
#endif /* MULIB_BROWSER_REPLAY_H */
