/* MuLIB browser staging addition: microjs-realm
 * Implemented subset: Bounded realm registry with global and intrinsic handles.
 * Remaining: Intrinsics installation, execution contexts, isolation and actual MicroJS binding.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_REALM_H
#define MULIB_MICROJS_REALM_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id;uintptr_t global,intrinsics;int active;} mulib_microjs_realm_entry;
typedef struct {mulib_microjs_realm_entry *v;size_t n,cap;uint64_t next;} mulib_microjs_realm_registry;
static inline int mulib_microjs_realm_create(mulib_microjs_realm_registry *r,uintptr_t global,uintptr_t intrinsics,uint64_t *out){if(!r||!out||!global||r->n>=r->cap||r->next==UINT64_MAX)return 0;*out=++r->next;r->v[r->n++]=(mulib_microjs_realm_entry){*out,global,intrinsics,1};return 1;}
static inline mulib_microjs_realm_entry *mulib_microjs_realm_find(mulib_microjs_realm_registry *r,uint64_t id){if(r)for(size_t i=0;i<r->n;i++)if(r->v[i].id==id&&r->v[i].active)return &r->v[i];return NULL;}
#endif /* MULIB_MICROJS_REALM_H */
