/* MuLIB browser staging addition: web-device
 * Implemented subset: Capability-scoped provider invocation for device handle and byte request.
 * Remaining: HID/USB/Bluetooth/serial protocols, chooser, security and permissions.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_DEVICE_H
#define MULIB_WEB_DEVICE_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_device_call_fn)(void *,uint64_t,unsigned,muba_span,muba_buf *);
static inline int mulib_web_device_call(int permitted,mulib_web_device_call_fn fn,void *ctx,uint64_t device,unsigned op,muba_span request,muba_buf *out){if(!permitted||!fn||!device||!out||!muba_span_ok(request))return 0;muba_buf b={0};if(!fn(ctx,device,op,request,&b)){muba_free(&b);return 0;}return muba_commit(out,&b);}
#endif /* MULIB_WEB_DEVICE_H */
