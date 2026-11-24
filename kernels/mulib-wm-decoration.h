/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_DECORATION_H
#define MULIB_WM_DECORATION_H
#include "mulib-wm-core.h"
typedef struct {int left,top,right,bottom;} mulib_wm_extents;
static inline int mulib_wm_decoration_content(mulib_wm_rect frame,mulib_wm_extents e,mulib_wm_rect *out){if(!out||!mulib_wm_rect_valid(frame)||e.left<0||e.top<0||e.right<0||e.bottom<0||(int64_t)e.left+e.right>frame.w||(int64_t)e.top+e.bottom>frame.h)return 0;*out=(mulib_wm_rect){frame.x+e.left,frame.y+e.top,frame.w-e.left-e.right,frame.h-e.top-e.bottom};return 1;}
enum {MULIB_WM_EDGE_LEFT=1,MULIB_WM_EDGE_TOP=2,MULIB_WM_EDGE_RIGHT=4,MULIB_WM_EDGE_BOTTOM=8};
static inline unsigned mulib_wm_decoration_hit(mulib_wm_rect r,int border,double x,double y){if(border<0||!mulib_wm_contains(r,x,y))return 0;return (x-r.x<border?1u:0u)|(y-r.y<border?2u:0u)|(r.x+(double)r.w-x<=border?4u:0u)|(r.y+(double)r.h-y<=border?8u:0u);}
#endif
