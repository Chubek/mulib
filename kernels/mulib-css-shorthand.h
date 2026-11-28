/* MuLIB browser staging addition: css-shorthand
 * Implemented subset: Expand one to four CSS box-side values in top/right/bottom/left order.
 * Remaining: Token grouping, CSS-wide keywords and property-specific expansion.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_SHORTHAND_H
#define MULIB_CSS_SHORTHAND_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_shorthand_sides(const muba_span *v,size_t n,muba_span out[4]){if(!v||!out||!n||n>4)return 0;for(size_t i=0;i<n;i++)if(!muba_span_ok(v[i]))return 0;muba_span t[4]={v[0],v[n>1?1:0],v[n>2?2:0],v[n>3?3:n>1?1:0]};memcpy(out,t,sizeof t);return 1;}
#endif /* MULIB_CSS_SHORTHAND_H */
