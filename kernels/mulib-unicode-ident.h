/* MuLIB browser staging addition: unicode-ident
 * Implemented subset: Generated ID_Start and ID_Continue Unicode property range lookup, with ECMAScript dollar, underscore and join controls.
 * Remaining: Pin the Unicode data version to ECMA-262 requirements, regenerate from official files and integrate escape-aware lexing.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_UNICODE_IDENT_H
#define MULIB_UNICODE_IDENT_H
#include "detail/mulib-browser-additions-common.h"
#include "detail/mulib-unicode-ident-data.inc"
static inline int mulib_unicode_ident_has(const muba_unicode_ident_range *v,size_t n,uint32_t c){size_t a=0,b=n;while(a<b){size_t m=a+(b-a)/2;if(c<v[m].first)b=m;else if(c>v[m].last)a=m+1;else return 1;}return 0;}
static inline int mulib_unicode_ident_start(uint32_t c){return c=='$'||c=='_'||mulib_unicode_ident_has(muba_unicode_ident_start,sizeof muba_unicode_ident_start/sizeof *muba_unicode_ident_start,c);}
static inline int mulib_unicode_ident_continue(uint32_t c){return c=='$'||c=='_'||c==0x200c||c==0x200d||mulib_unicode_ident_has(muba_unicode_ident_continue,sizeof muba_unicode_ident_continue/sizeof *muba_unicode_ident_continue,c);}
#endif /* MULIB_UNICODE_IDENT_H */
