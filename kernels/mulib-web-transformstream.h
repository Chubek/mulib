/* MuLIB browser staging addition: web-transformstream
 * Implemented subset: Synchronous byte transform callback with bounded output.
 * Remaining: Asynchronous transform controller, queueing and JS stream integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_TRANSFORMSTREAM_H
#define MULIB_WEB_TRANSFORMSTREAM_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_transformstream_fn)(void *,muba_span,muba_buf *);
static inline int mulib_web_transformstream_apply(mulib_web_transformstream_fn f,void *ctx,muba_span in,size_t limit,muba_buf *out){if(!f||!out||!muba_span_ok(in))return 0;muba_buf b={0};if(!f(ctx,in,&b)||b.len>limit){muba_free(&b);return 0;}return muba_commit(out,&b);}
#endif /* MULIB_WEB_TRANSFORMSTREAM_H */
