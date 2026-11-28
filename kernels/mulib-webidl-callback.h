/* MuLIB browser staging addition: webidl-callback
 * Implemented subset: Host callback invocation with non-reentrant guard.
 * Remaining: JS callable validation, realm entry, exceptions and callback lifetime.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_CALLBACK_H
#define MULIB_WEBIDL_CALLBACK_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_webidl_callback_fn)(void *,const uintptr_t *,size_t,uintptr_t *);
typedef struct {mulib_webidl_callback_fn fn;void *ctx;int active;} mulib_webidl_callback_t;
static inline int mulib_webidl_callback_call(mulib_webidl_callback_t *c,const uintptr_t *args,size_t n,uintptr_t *result){if(!c||!c->fn||c->active||(!args&&n)||!result)return 0;c->active=1;int r=c->fn(c->ctx,args,n,result);c->active=0;return r;}
#endif /* MULIB_WEBIDL_CALLBACK_H */
