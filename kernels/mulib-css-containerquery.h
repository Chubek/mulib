/* MuLIB browser staging addition: css-containerquery
 * Implemented subset: Select nearest eligible size container through ancestor records.
 * Remaining: Container names, style queries, containment and invalidation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_CONTAINERQUERY_H
#define MULIB_CSS_CONTAINERQUERY_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id;muba_span name;unsigned axes;} mulib_css_containerquery_container;
static inline uint64_t mulib_css_containerquery_nearest(const mulib_css_containerquery_container *v,size_t n,muba_span name,unsigned axes){if(v)for(size_t i=0;i<n;i++)if((v[i].axes&axes)==axes&&(!name.len||muba_eq(v[i].name,name)))return v[i].id;return 0;}
#endif /* MULIB_CSS_CONTAINERQUERY_H */
