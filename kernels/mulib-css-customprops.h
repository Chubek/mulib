/* MuLIB browser staging addition: css-customprops
 * Implemented subset: Lookup of borrowed custom property values by exact case-sensitive name.
 * Remaining: Cascade, recursive var substitution, cycles, fallback and registered properties.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_CUSTOMPROPS_H
#define MULIB_CSS_CUSTOMPROPS_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span name,value;} mulib_css_customprops_entry;
static inline const mulib_css_customprops_entry *mulib_css_customprops_find(const mulib_css_customprops_entry *v,size_t n,muba_span name){if(!muba_span_ok(name)||name.len<3||name.data[0]!='-'||name.data[1]!='-')return NULL;while(v&&n){n--;if(muba_eq(v[n].name,name))return v+n;}return NULL;}
#endif /* MULIB_CSS_CUSTOMPROPS_H */
