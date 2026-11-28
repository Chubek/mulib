/* MuLIB browser staging addition: web-worker
 * Implemented subset: Worker lifecycle with start and terminate callbacks.
 * Remaining: Thread/agent creation, script fetching, messaging, errors and termination.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WORKER_H
#define MULIB_WEB_WORKER_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_web_worker_NEW,mulib_web_worker_RUNNING,mulib_web_worker_STOPPED} mulib_web_worker_state;
typedef struct {mulib_web_worker_state state;int (*start)(void *);void (*stop)(void *);void *ctx;int busy;} mulib_web_worker_t;
static inline int mulib_web_worker_start(mulib_web_worker_t *w){if(!w||w->busy||w->state!=mulib_web_worker_NEW||!w->start)return 0;w->busy=1;int ok=w->start(w->ctx);w->busy=0;if(ok)w->state=mulib_web_worker_RUNNING;return ok;}
static inline int mulib_web_worker_terminate(mulib_web_worker_t *w){if(!w||w->busy||w->state!=mulib_web_worker_RUNNING)return 0;w->state=mulib_web_worker_STOPPED;if(w->stop)w->stop(w->ctx);return 1;}
#endif /* MULIB_WEB_WORKER_H */
