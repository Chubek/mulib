/* MuLIB browser staging addition: browser-replay
 * Implemented subset: Ordered replay of retained journal events after sequence cursor; stop on rejection.
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
#endif /* MULIB_BROWSER_REPLAY_H */
