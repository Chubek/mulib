/* MuLIB browser staging addition: fetch-headers
 * Implemented subset: Validate HTTP header names and values, normalized case-insensitive lookup.
 * Remaining: Header list combination, guards, forbidden headers and structured parsing.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_HEADERS_H
#define MULIB_FETCH_HEADERS_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span name,value;} mulib_fetch_headers_field;
static inline int mulib_fetch_headers_valid(mulib_fetch_headers_field f){if(!muba_span_ok(f.name)||!muba_span_ok(f.value)||!f.name.len)return 0;for(size_t i=0;i<f.name.len;i++){unsigned char c=f.name.data[i];if(!c||(!muba_alpha(c)&&!muba_digit(c)&&!strchr("!#$%&'*+-.^_`|~",c)))return 0;}for(size_t i=0;i<f.value.len;i++){unsigned char c=f.value.data[i];if(c==0||c=='\r'||c=='\n')return 0;}return 1;}
static inline const mulib_fetch_headers_field *mulib_fetch_headers_find(const mulib_fetch_headers_field *v,size_t n,muba_span name){if(v)for(size_t i=0;i<n;i++)if(muba_ieq(v[i].name,name))return v+i;return NULL;}
#endif /* MULIB_FETCH_HEADERS_H */
