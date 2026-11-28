/* MuLIB browser staging addition: web-webgpu
 * Implemented subset: Validate buffer range and alignment and forward host buffer write.
 * Remaining: GPU device, queue, shader validation, resource limits and error scopes.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WEBGPU_H
#define MULIB_WEB_WEBGPU_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_webgpu_write_fn)(void *,uint64_t,size_t,muba_span);
static inline int mulib_web_webgpu_write(mulib_web_webgpu_write_fn fn,void *ctx,uint64_t buffer,size_t size,size_t offset,muba_span data){return fn&&buffer&&muba_span_ok(data)&&offset%4==0&&data.len%4==0&&offset<=size&&data.len<=size-offset&&fn(ctx,buffer,offset,data);}
#endif /* MULIB_WEB_WEBGPU_H */
