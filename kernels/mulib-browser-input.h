/* MuLIB browser staging addition: browser-input
 * Implemented subset: Validated pointer input dispatch to host hit-test and event callback.
 * Remaining: Trusted input, focus, default actions, capture and keyboard.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_INPUT_H
#define MULIB_BROWSER_INPUT_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-web-input.h"
typedef uint64_t (*mulib_browser_input_hit_fn)(void *,double,double);typedef int (*mulib_browser_input_dispatch_fn)(void *,uint64_t,mulib_web_input_pointer);
static inline int mulib_browser_input_pointer(mulib_browser_input_hit_fn hit,mulib_browser_input_dispatch_fn dispatch,void *ctx,mulib_web_input_pointer e){if(!hit||!dispatch||!mulib_web_input_valid(e))return 0;uint64_t id=hit(ctx,e.x,e.y);return id&&dispatch(ctx,id,e);}
#endif /* MULIB_BROWSER_INPUT_H */
