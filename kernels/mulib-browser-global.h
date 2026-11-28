/* MuLIB browser staging addition: browser-global
 * Implemented subset: Feature-gated global installation through host callback.
 * Remaining: Window/Worker globals, actual JS descriptors and exposure.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_GLOBAL_H
#define MULIB_BROWSER_GLOBAL_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint32_t id;unsigned exposure;int secure;} mulib_browser_global_entry;
typedef int (*mulib_browser_global_install_fn)(void *,uint32_t);
static inline int mulib_browser_global_install(const mulib_browser_global_entry *v,size_t n,unsigned exposure,int secure,mulib_browser_global_install_fn fn,void *ctx){if(!fn||(!v&&n))return 0;for(size_t i=0;i<n;i++)if((v[i].exposure&exposure)&&(!v[i].secure||secure)&&!fn(ctx,v[i].id))return 0;return 1;}
#endif /* MULIB_BROWSER_GLOBAL_H */
