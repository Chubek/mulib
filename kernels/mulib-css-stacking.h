/* MuLIB browser staging addition: css-stacking
 * Implemented subset: Stable z-index ordering within one caller-defined stacking context.
 * Remaining: Stacking context creation, paint phases and top layer.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_STACKING_H
#define MULIB_CSS_STACKING_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t node;int32_t z;} mulib_css_stacking_entry;
static inline void mulib_css_stacking_sort(mulib_css_stacking_entry *v,size_t n){if(v)for(size_t i=1;i<n;i++){mulib_css_stacking_entry x=v[i];size_t j=i;while(j&&v[j-1].z>x.z){v[j]=v[j-1];j--;}v[j]=x;}}
#endif /* MULIB_CSS_STACKING_H */
