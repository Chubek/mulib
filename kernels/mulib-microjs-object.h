/* MuLIB browser staging addition: microjs-object
 * Implemented subset: Bounded own data descriptors and prototype-chain lookup; nonwritable/nonconfigurable enforcement.
 * Remaining: Accessors, symbols, proxy/exotic semantics, GC and MicroJS adapter.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_OBJECT_H
#define MULIB_MICROJS_OBJECT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span key;uintptr_t value;unsigned writable:1, enumerable:1, configurable:1;} mulib_microjs_object_prop;
typedef struct mulib_microjs_object_obj {struct mulib_microjs_object_obj *proto;mulib_microjs_object_prop *props;size_t len,cap;int extensible;} mulib_microjs_object_obj;
static inline const mulib_microjs_object_prop *mulib_microjs_object_own(const mulib_microjs_object_obj *o,muba_span key){if(!o||!muba_span_ok(key))return NULL;for(size_t i=0;i<o->len;i++)if(muba_eq(o->props[i].key,key))return &o->props[i];return NULL;}
static inline const mulib_microjs_object_prop *mulib_microjs_object_get(const mulib_microjs_object_obj *o,muba_span key,size_t budget){while(o&&budget--){const mulib_microjs_object_prop *p=mulib_microjs_object_own(o,key);if(p)return p;o=o->proto;}return NULL;}
static inline int mulib_microjs_object_define(mulib_microjs_object_obj *o,mulib_microjs_object_prop p){if(!o||!muba_span_ok(p.key))return 0;for(size_t i=0;i<o->len;i++)if(muba_eq(o->props[i].key,p.key)){mulib_microjs_object_prop old=o->props[i];if(!old.configurable&&(p.configurable||p.enumerable!=old.enumerable||(!old.writable&&(p.writable||p.value!=old.value))))return 0;o->props[i]=p;return 1;}if(!o->extensible||o->len>=o->cap)return 0;o->props[o->len++]=p;return 1;}
static inline int mulib_microjs_object_set_proto(mulib_microjs_object_obj *o,mulib_microjs_object_obj *p,size_t budget){if(!o)return 0;if(o->proto==p)return 1;if(!o->extensible)return 0;for(mulib_microjs_object_obj *q=p;q;q=q->proto){if(!budget--||q==o)return 0;}o->proto=p;return 1;}
#endif /* MULIB_MICROJS_OBJECT_H */
