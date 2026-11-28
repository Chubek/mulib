/* MuLIB browser staging addition: microjs-async
 * Implemented subset: One-shot deferred continuation posted to a caller-provided queue.
 * Remaining: Promise integration, rejection propagation and suspended VM frames.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_ASYNC_H
#define MULIB_MICROJS_ASYNC_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_job_fn resume;void *ctx;int posted;} mulib_microjs_async_continuation;
static inline int mulib_microjs_async_schedule(mulib_microjs_async_continuation *c,muba_queue *q){if(!c||c->posted||!muba_queue_push(q,c->resume,c->ctx))return 0;c->posted=1;return 1;}
#endif /* MULIB_MICROJS_ASYNC_H */
