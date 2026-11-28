/* MuLIB browser staging addition: webidl-union
 * Implemented subset: Unique type-mask branch selection.
 * Remaining: WebIDL type precedence, nullable/dictionary conversions and JS runtime.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_UNION_H
#define MULIB_WEBIDL_UNION_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_webidl_union_select(const uint32_t *masks,size_t n,uint32_t type,size_t *out){if(!out||(!masks&&n)||!type)return 0;size_t count=0,x=0;for(size_t i=0;i<n;i++)if(masks[i]&type){count++;x=i;}if(count!=1)return 0;*out=x;return 1;}
#endif /* MULIB_WEBIDL_UNION_H */
