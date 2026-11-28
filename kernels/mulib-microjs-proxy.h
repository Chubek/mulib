/* MuLIB browser staging addition: microjs-proxy
 * Implemented subset: Get-trap invariant for nonconfigurable nonwritable data properties.
 * Remaining: All internal methods, accessor invariants, revocation and Reflect.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_PROXY_H
#define MULIB_MICROJS_PROXY_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-microjs-object.h"
static inline int mulib_microjs_proxy_get_allowed(const mulib_microjs_object_prop *p,uintptr_t result){return !p||p->configurable||p->writable||p->value==result;}
#endif /* MULIB_MICROJS_PROXY_H */
