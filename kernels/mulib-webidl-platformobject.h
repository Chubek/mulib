/* MuLIB browser staging addition: webidl-platformobject
 * Implemented subset: Native object brand check with bounded interface inheritance chain.
 * Remaining: JS internal methods, property descriptors, exotic indexed/named objects.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_PLATFORMOBJECT_H
#define MULIB_WEBIDL_PLATFORMOBJECT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct mulib_webidl_platformobject_interface {uint32_t id;const struct mulib_webidl_platformobject_interface *parent;} mulib_webidl_platformobject_interface;
typedef struct {uintptr_t native;const mulib_webidl_platformobject_interface *type;} mulib_webidl_platformobject_t;
static inline int mulib_webidl_platformobject_is(const mulib_webidl_platformobject_t *o,uint32_t id,size_t budget){if(!o||!o->native||!id)return 0;for(const mulib_webidl_platformobject_interface *p=o->type;p&&budget--;p=p->parent)if(p->id==id)return 1;return 0;}
#endif /* MULIB_WEBIDL_PLATFORMOBJECT_H */
