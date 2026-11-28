/* MuLIB browser staging addition: web-abort
 * Implemented subset: One-shot abort with reason and bounded listener callbacks.
 * Remaining: AbortSignal.any/timeout, event targets, removal and JS exceptions.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_ABORT_H
#define MULIB_WEB_ABORT_H
#include "detail/mulib-browser-additions-common.h"

typedef void (*mulib_web_abort_fn)(void *,uintptr_t);
typedef struct {mulib_web_abort_fn fn;void *ctx;} mulib_web_abort_listener;
typedef struct {mulib_web_abort_listener *v;size_t len,cap;int aborted;uintptr_t reason;} mulib_web_abort_t;
static inline int mulib_web_abort_listen(mulib_web_abort_t *s,mulib_web_abort_fn fn,void *ctx){if(!s||!fn||s->aborted||s->len>=s->cap)return 0;s->v[s->len++]=(mulib_web_abort_listener){fn,ctx};return 1;}
static inline int mulib_web_abort_abort(mulib_web_abort_t *s,uintptr_t reason){if(!s||s->aborted)return 0;s->aborted=1;s->reason=reason;for(size_t i=0;i<s->len;i++)s->v[i].fn(s->v[i].ctx,reason);return 1;}
#endif /* MULIB_WEB_ABORT_H */
