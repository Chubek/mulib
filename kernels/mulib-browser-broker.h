/* MuLIB browser staging addition: browser-broker
 * Implemented subset: Explicit capability mask gate and bounded provider request dispatch.
 * Remaining: Unforgeable capabilities, IPC validation, sandbox and auditing.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_BROKER_H
#define MULIB_BROWSER_BROKER_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_browser_broker_fn)(void *,unsigned,muba_span,muba_buf *);
static inline int mulib_browser_broker_request(uint64_t capabilities,unsigned op,mulib_browser_broker_fn fn,void *ctx,muba_span request,size_t limit,muba_buf *out){if(op>=64||!(capabilities&(UINT64_C(1)<<op))||!fn||!out||!muba_span_ok(request)||request.len>limit)return 0;muba_buf b={0};if(!fn(ctx,op,request,&b)||b.len>limit){muba_free(&b);return 0;}return muba_commit(out,&b);}
#endif /* MULIB_BROWSER_BROKER_H */
