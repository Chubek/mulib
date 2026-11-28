/* MuLIB browser staging addition: dom-shadow
 * Implemented subset: Host/root attachment record with open/closed mode.
 * Remaining: Tree integration, root lifetime, event retargeting and shadow DOM rules.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_DOM_SHADOW_H
#define MULIB_DOM_SHADOW_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t host,root;int closed;} mulib_dom_shadow_t;
static inline int mulib_dom_shadow_attach(mulib_dom_shadow_t *s,uint64_t host,uint64_t root,int closed){if(!s||s->host||!host||!root||host==root)return 0;*s=(mulib_dom_shadow_t){host,root,!!closed};return 1;}
static inline uint64_t mulib_dom_shadow_visible(const mulib_dom_shadow_t *s){return s&&!s->closed?s->root:0;}
#endif /* MULIB_DOM_SHADOW_H */
