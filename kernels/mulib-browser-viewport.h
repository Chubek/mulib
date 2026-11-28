/* MuLIB browser staging addition: browser-viewport
 * Implemented subset: Validated viewport dimensions and bounded scroll offsets.
 * Remaining: Zoom, visual viewport, screen metrics, events and layout.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_VIEWPORT_H
#define MULIB_BROWSER_VIEWPORT_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-css-overflow.h"
typedef struct {double width,height,scale,x,y;} mulib_browser_viewport_t;
static inline int mulib_browser_viewport_resize(mulib_browser_viewport_t *v,double w,double h,double scale){if(!v||!isfinite(w)||!isfinite(h)||!isfinite(scale)||w<0||h<0||scale<=0)return 0;v->width=w;v->height=h;v->scale=scale;return 1;}
static inline int mulib_browser_viewport_scroll(mulib_browser_viewport_t *v,double x,double y,double cw,double ch){if(!v)return 0;double a,b;if(!mulib_css_overflow_scroll(v->width,cw,x,&a)||!mulib_css_overflow_scroll(v->height,ch,y,&b))return 0;v->x=a;v->y=b;return 1;}
#endif /* MULIB_BROWSER_VIEWPORT_H */
