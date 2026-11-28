/* MuLIB browser staging addition: browser-context
 * Implemented subset: Bounded context registry with parent validation and child-free removal.
 * Remaining: Nested context lifecycle, browsing context groups and WindowProxy integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_CONTEXT_H
#define MULIB_BROWSER_CONTEXT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id,parent,document;} mulib_browser_context_entry;
static inline const mulib_browser_context_entry *mulib_browser_context_find(const mulib_browser_context_entry *v,size_t n,uint64_t id){if(v)for(size_t i=0;i<n;i++)if(v[i].id==id)return v+i;return NULL;}
static inline int mulib_browser_context_add(mulib_browser_context_entry *v,size_t *n,size_t cap,mulib_browser_context_entry e){if(!v||!n||*n>=cap||!e.id||mulib_browser_context_find(v,*n,e.id)||(e.parent&&!mulib_browser_context_find(v,*n,e.parent)))return 0;v[(*n)++]=e;return 1;}
static inline int mulib_browser_context_remove(mulib_browser_context_entry *v,size_t *n,uint64_t id){if(!v||!n||!id)return 0;for(size_t i=0;i<*n;i++)if(v[i].parent==id)return 0;for(size_t i=0;i<*n;i++)if(v[i].id==id){v[i]=v[--*n];return 1;}return 0;}
#endif /* MULIB_BROWSER_CONTEXT_H */
