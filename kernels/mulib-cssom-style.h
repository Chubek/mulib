/* MuLIB browser staging addition: cssom-style
 * Implemented subset: Bounded borrowed declaration list with replace/remove and priority bit.
 * Remaining: CSS parsing, shorthand semantics, serialization and live CSSOM binding.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSSOM_STYLE_H
#define MULIB_CSSOM_STYLE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span name,value;int important;} mulib_cssom_style_decl;
static inline int mulib_cssom_style_set(mulib_cssom_style_decl *v,size_t *n,size_t cap,mulib_cssom_style_decl d){if(!v||!n||!d.name.len||!muba_span_ok(d.name)||!muba_span_ok(d.value))return 0;for(size_t i=0;i<*n;i++)if(muba_eq(v[i].name,d.name)){v[i]=d;return 1;}if(*n>=cap)return 0;v[(*n)++]=d;return 1;}
static inline int mulib_cssom_style_remove(mulib_cssom_style_decl *v,size_t *n,muba_span name){if(!v||!n)return 0;for(size_t i=0;i<*n;i++)if(muba_eq(v[i].name,name)){if(i+1<*n)memmove(v+i,v+i+1,(*n-i-1)*sizeof *v);(*n)--;return 1;}return 0;}
#endif /* MULIB_CSSOM_STYLE_H */
