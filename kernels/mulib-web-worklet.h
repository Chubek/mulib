/* MuLIB browser staging addition: web-worklet
 * Implemented subset: Bounded callback dispatch by registered processor name.
 * Remaining: Worklet modules, isolated realms, audio/paint scheduling and deadlines.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WORKLET_H
#define MULIB_WEB_WORKLET_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_worklet_fn)(void *,uintptr_t);
typedef struct {muba_span name;mulib_web_worklet_fn fn;void *ctx;} mulib_web_worklet_entry;
static inline int mulib_web_worklet_run(const mulib_web_worklet_entry *v,size_t n,muba_span name,uintptr_t arg){if(v)for(size_t i=0;i<n;i++)if(v[i].fn&&muba_eq(v[i].name,name))return v[i].fn(v[i].ctx,arg);return 0;}
#endif /* MULIB_WEB_WORKLET_H */
