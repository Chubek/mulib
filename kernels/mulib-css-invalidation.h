/* MuLIB browser staging addition: css-invalidation
 * Implemented subset: Deduplicated node dirty-bit queue with bounded capacity.
 * Remaining: Selector dependency tracking, subtree invalidation and incremental restyle.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_INVALIDATION_H
#define MULIB_CSS_INVALIDATION_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t node;unsigned flags;} mulib_css_invalidation_entry;
static inline int mulib_css_invalidation_mark(mulib_css_invalidation_entry *v,size_t *n,size_t cap,uint64_t node,unsigned flags){if(!v||!n||!node||!flags)return 0;for(size_t i=0;i<*n;i++)if(v[i].node==node){v[i].flags|=flags;return 1;}if(*n>=cap)return 0;v[(*n)++]=(mulib_css_invalidation_entry){node,flags};return 1;}
#endif /* MULIB_CSS_INVALIDATION_H */
