/* MuLIB browser staging addition: web-formdata
 * Implemented subset: Ordered borrowed name/value entries and duplicate-aware append.
 * Remaining: Blob/file entries, multipart generation, set/delete and JS iteration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_FORMDATA_H
#define MULIB_WEB_FORMDATA_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span name,value;} mulib_web_formdata_entry;
static inline int mulib_web_formdata_append(mulib_web_formdata_entry *v,size_t *n,size_t cap,mulib_web_formdata_entry e){if(!v||!n||*n>=cap||!muba_span_ok(e.name)||!muba_span_ok(e.value))return 0;v[(*n)++]=e;return 1;}
static inline const mulib_web_formdata_entry *mulib_web_formdata_get(const mulib_web_formdata_entry *v,size_t n,muba_span name){if(v)for(size_t i=0;i<n;i++)if(muba_eq(v[i].name,name))return v+i;return NULL;}
#endif /* MULIB_WEB_FORMDATA_H */
