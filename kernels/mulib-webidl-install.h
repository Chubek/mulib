/* MuLIB browser staging addition: webidl-install
 * Implemented subset: Ordered interface installation through host callback; stop on first failure.
 * Remaining: Transactional realm bootstrap, prototype construction and generated bindings.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_INSTALL_H
#define MULIB_WEBIDL_INSTALL_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_webidl_install_fn)(void *,uint32_t);
static inline size_t mulib_webidl_install_run(const uint32_t *ids,size_t n,mulib_webidl_install_fn fn,void *ctx){if(!fn||(!ids&&n))return 0;size_t i=0;while(i<n&&fn(ctx,ids[i]))i++;return i;}
#endif /* MULIB_WEBIDL_INSTALL_H */
