/* MuLIB browser staging addition: web-writablestream
 * Implemented subset: Synchronous bounded sink writes with close/failure state.
 * Remaining: Async sink algorithms, locks, backpressure and promises.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WRITABLESTREAM_H
#define MULIB_WEB_WRITABLESTREAM_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_writablestream_sink)(void *,muba_span);
typedef struct {mulib_web_writablestream_sink sink;void *ctx;size_t total,limit;int closed,error,active;} mulib_web_writablestream_t;
static inline int mulib_web_writablestream_write(mulib_web_writablestream_t *s,muba_span v){if(!s||!s->sink||s->closed||s->error||s->active||!muba_span_ok(v)||s->total>s->limit||v.len>s->limit-s->total)return 0;s->active=1;int ok=s->sink(s->ctx,v);s->active=0;if(!ok){s->error=1;return 0;}s->total+=v.len;return 1;}
static inline void mulib_web_writablestream_close(mulib_web_writablestream_t *s){if(s&&!s->active)s->closed=1;}
#endif /* MULIB_WEB_WRITABLESTREAM_H */
