/* MuLIB browser staging addition: browser-geometryquery
 * Implemented subset: Explicit synchronous geometry provider with finite rectangle validation.
 * Remaining: Layout flush, transforms, fragmentation and renderer synchronization.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_GEOMETRYQUERY_H
#define MULIB_BROWSER_GEOMETRYQUERY_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {double x,y,w,h;} mulib_browser_geometryquery_rect;
typedef int (*mulib_browser_geometryquery_query_fn)(void *,uint64_t,mulib_browser_geometryquery_rect *);
static inline int mulib_browser_geometryquery_get(mulib_browser_geometryquery_query_fn q,void *ctx,uint64_t node,mulib_browser_geometryquery_rect *out){if(!q||!node||!out)return 0;mulib_browser_geometryquery_rect r={0};if(!q(ctx,node,&r)||!isfinite(r.x)||!isfinite(r.y)||!isfinite(r.w)||!isfinite(r.h)||r.w<0||r.h<0)return 0;*out=r;return 1;}
#endif /* MULIB_BROWSER_GEOMETRYQUERY_H */
