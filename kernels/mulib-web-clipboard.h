/* MuLIB browser staging addition: web-clipboard
 * Implemented subset: Permission-gated text write provider dispatch.
 * Remaining: Read, MIME formats, user activation and secure context enforcement.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_CLIPBOARD_H
#define MULIB_WEB_CLIPBOARD_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_clipboard_write_fn)(void *,muba_span);
static inline int mulib_web_clipboard_write(int permitted,mulib_web_clipboard_write_fn fn,void *ctx,muba_span text,size_t limit){return permitted&&fn&&muba_span_ok(text)&&text.len<=limit&&fn(ctx,text);}
#endif /* MULIB_WEB_CLIPBOARD_H */
