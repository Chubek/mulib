/* MuLIB browser staging addition: web-notification
 * Implemented subset: Permission-gated notification provider dispatch.
 * Remaining: Permission prompts, lifecycle, actions and service worker events.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_NOTIFICATION_H
#define MULIB_WEB_NOTIFICATION_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_notification_show_fn)(void *,muba_span,muba_span);
static inline int mulib_web_notification_show(int allowed,mulib_web_notification_show_fn fn,void *ctx,muba_span title,muba_span body){return allowed&&fn&&muba_span_ok(title)&&muba_span_ok(body)&&fn(ctx,title,body);}
#endif /* MULIB_WEB_NOTIFICATION_H */
