/* MuLIB browser staging addition: web-messageport
 * Implemented subset: Bounded queued host message handles and close state.
 * Remaining: Entanglement transfer, structured clone, event loop and worker delivery.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_MESSAGEPORT_H
#define MULIB_WEB_MESSAGEPORT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uintptr_t *v;size_t cap,head,len;int closed;} mulib_web_messageport_t;
static inline int mulib_web_messageport_post(mulib_web_messageport_t *p,uintptr_t msg){if(!p||p->closed||p->len>=p->cap)return 0;p->v[(p->head+p->len)%p->cap]=msg;p->len++;return 1;}
static inline int mulib_web_messageport_receive(mulib_web_messageport_t *p,uintptr_t *msg){if(!p||!msg||!p->len)return 0;*msg=p->v[p->head];p->head=(p->head+1)%p->cap;p->len--;return 1;}
#endif /* MULIB_WEB_MESSAGEPORT_H */
