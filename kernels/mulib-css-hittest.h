/* MuLIB browser staging addition: css-hittest
 * Implemented subset: Reverse paint-order axis-aligned rectangle hit test.
 * Remaining: Clipping, transforms, pointer-events, shadow retargeting and scrolling.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_HITTEST_H
#define MULIB_CSS_HITTEST_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id;double x,y,w,h;int enabled;} mulib_css_hittest_box;
static inline uint64_t mulib_css_hittest_hit(const mulib_css_hittest_box *v,size_t n,double x,double y){if(v&&isfinite(x)&&isfinite(y))while(n){const mulib_css_hittest_box *b=v+--n;if(b->enabled&&b->w>0&&b->h>0&&x>=b->x&&y>=b->y&&x-b->x<b->w&&y-b->y<b->h)return b->id;}return 0;}
#endif /* MULIB_CSS_HITTEST_H */
