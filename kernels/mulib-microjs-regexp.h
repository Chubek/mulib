/* MuLIB browser staging addition: microjs-regexp
 * Implemented subset: Literal UTF-16 substring matching with sticky mode and ASCII case folding.
 * Remaining: Full RegExp grammar, backtracking, Unicode properties, flags and capture groups.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_REGEXP_H
#define MULIB_MICROJS_REGEXP_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_microjs_regexp_literal(const uint16_t *s,size_t n,const uint16_t *p,size_t m,size_t start,int sticky,int fold,size_t *at){if(!at||(!s&&n)||(!p&&m)||start>n||m>n-start)return 0;for(size_t i=start;i<=n-m;i++){size_t j=0;for(;j<m;j++){uint16_t a=s[i+j],b=p[j];if(fold){if(a<128)a=muba_lower((unsigned char)a);if(b<128)b=muba_lower((unsigned char)b);}if(a!=b)break;}if(j==m){*at=i;return 1;}if(sticky)break;}return 0;}
#endif /* MULIB_MICROJS_REGEXP_H */
