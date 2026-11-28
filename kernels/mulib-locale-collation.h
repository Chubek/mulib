/* MuLIB browser staging addition: locale-collation
 * Implemented subset: ASCII case-insensitive lexicographic comparison.
 * Remaining: Unicode Collation Algorithm, CLDR tailoring and normalization.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_LOCALE_COLLATION_H
#define MULIB_LOCALE_COLLATION_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_locale_collation_compare(muba_span a,muba_span b,int fold){if(!muba_span_ok(a)||!muba_span_ok(b))return 0;size_t n=a.len<b.len?a.len:b.len;for(size_t i=0;i<n;i++){unsigned x=a.data[i],y=b.data[i];if(fold){x=muba_lower((unsigned char)x);y=muba_lower((unsigned char)y);}if(x!=y)return x<y?-1:1;}return (a.len>b.len)-(a.len<b.len);}
#endif /* MULIB_LOCALE_COLLATION_H */
