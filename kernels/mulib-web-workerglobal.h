/* MuLIB browser staging addition: web-workerglobal
 * Implemented subset: Worker global environment metadata and exposure check.
 * Remaining: Real JS globals, event target, timer/fetch and importScripts.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WORKERGLOBAL_H
#define MULIB_WEB_WORKERGLOBAL_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t realm,agent;unsigned exposed;int closing;} mulib_web_workerglobal_t;
static inline int mulib_web_workerglobal_exposed(const mulib_web_workerglobal_t *w,unsigned mask){return w&&w->realm&&!w->closing&&(w->exposed&mask)!=0;}
#endif /* MULIB_WEB_WORKERGLOBAL_H */
