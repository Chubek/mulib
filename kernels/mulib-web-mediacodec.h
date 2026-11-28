/* MuLIB browser staging addition: web-mediacodec
 * Implemented subset: Bounded encoded chunk submission to explicit decoder provider.
 * Remaining: WebCodecs queues, codec negotiation, frame lifetime and asynchronous callbacks.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_MEDIACODEC_H
#define MULIB_WEB_MEDIACODEC_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_mediacodec_decode)(void *,int64_t,int,muba_span);
static inline int mulib_web_mediacodec_submit(mulib_web_mediacodec_decode f,void *ctx,int64_t timestamp,int key,muba_span chunk,size_t cap){return f&&muba_span_ok(chunk)&&chunk.len<=cap&&f(ctx,timestamp,!!key,chunk);}
#endif /* MULIB_WEB_MEDIACODEC_H */
