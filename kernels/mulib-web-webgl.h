/* MuLIB browser staging addition: web-webgl
 * Implemented subset: Validate buffer upload span and dispatch to explicit graphics provider.
 * Remaining: WebGL context, complete GLES semantics, shader validation and security.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WEBGL_H
#define MULIB_WEB_WEBGL_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_webgl_upload)(void *,unsigned,muba_span);
static inline int mulib_web_webgl_buffer(mulib_web_webgl_upload fn,void *ctx,unsigned target,muba_span data,size_t cap){return fn&&muba_span_ok(data)&&data.len<=cap&&(target==0x8892||target==0x8893)&&fn(ctx,target,data);}
#endif /* MULIB_WEB_WEBGL_H */
