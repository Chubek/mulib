/* MuLIB browser staging addition: web-console
 * Implemented subset: Severity-tagged message forwarding to host.
 * Remaining: Console formatting, counters, groups and timers.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_CONSOLE_H
#define MULIB_WEB_CONSOLE_H
#include "detail/mulib-browser-additions-common.h"

typedef void (*mulib_web_console_sink)(void *,unsigned,muba_span);
static inline int mulib_web_console_log(mulib_web_console_sink sink,void *ctx,unsigned level,muba_span msg){if(!sink||level>4||!muba_span_ok(msg))return 0;sink(ctx,level,msg);return 1;}
#endif /* MULIB_WEB_CONSOLE_H */
