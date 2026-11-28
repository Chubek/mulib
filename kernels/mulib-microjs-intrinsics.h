/* MuLIB browser staging addition: microjs-intrinsics
 * Implemented subset: Bounded intrinsic registry with duplicate detection.
 * Remaining: Actual ECMA constructors/prototypes, descriptors and realm bootstrap.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_INTRINSICS_H
#define MULIB_MICROJS_INTRINSICS_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span name;uintptr_t handle;} mulib_microjs_intrinsics_entry;
static inline int mulib_microjs_intrinsics_install(mulib_microjs_intrinsics_entry *v,size_t *n,size_t cap,muba_span name,uintptr_t h){if(!v||!n||*n>=cap||!name.len||!muba_span_ok(name)||!h)return 0;for(size_t i=0;i<*n;i++)if(muba_eq(v[i].name,name))return 0;v[(*n)++]=(mulib_microjs_intrinsics_entry){name,h};return 1;}
static inline uintptr_t mulib_microjs_intrinsics_find(const mulib_microjs_intrinsics_entry *v,size_t n,muba_span name){if(v)for(size_t i=0;i<n;i++)if(muba_eq(v[i].name,name))return v[i].handle;return 0;}
#endif /* MULIB_MICROJS_INTRINSICS_H */
