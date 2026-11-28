/* MuLIB browser staging addition: browser-subscription
 * Implemented subset: Channel-filtered journal replay with persistent sequence cursor.
 * Remaining: Async queues, backpressure, unsubscribe and concurrent delivery.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_SUBSCRIPTION_H
#define MULIB_BROWSER_SUBSCRIPTION_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-browser-journal.h"
typedef struct {uint64_t after,mask;} mulib_browser_subscription_t;
typedef int (*mulib_browser_subscription_fn)(void *,const mulib_browser_journal_event *);
static inline int mulib_browser_subscription_poll(mulib_browser_subscription_t *s,const mulib_browser_journal_t *j,mulib_browser_subscription_fn fn,void *ctx){if(!s||!j||!fn)return 0;for(size_t i=0;i<j->len;i++){const mulib_browser_journal_event *e=j->v+i;if(e->seq<=s->after)continue;if(e->channel<64&&(s->mask&(UINT64_C(1)<<e->channel))&&!fn(ctx,e))return 0;s->after=e->seq;}return 1;}
#endif /* MULIB_BROWSER_SUBSCRIPTION_H */
