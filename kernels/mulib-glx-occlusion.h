/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_GLX_OCCLUSION_H
#define MULIB_GLX_OCCLUSION_H
#include "mulib-wm-core.h"
/* Subtract opaque cover, producing at most four disjoint nonempty rectangles. */
static inline size_t mulib_glx_occlusion_subtract(mulib_wm_rect a,mulib_wm_rect cover,mulib_wm_rect out[4]){if(!out||!mulib_wm_rect_valid(a)||!a.w||!a.h)return 0;mulib_wm_rect i=mulib_wm_intersect(a,cover);if(!i.w||!i.h){out[0]=a;return 1;}mulib_wm_rect p[4]={{a.x,a.y,a.w,i.y-a.y},{a.x,i.y+i.h,a.w,a.y+a.h-i.y-i.h},{a.x,i.y,i.x-a.x,i.h},{i.x+i.w,i.y,a.x+a.w-i.x-i.w,i.h}};size_t n=0;for(size_t k=0;k<4;k++)if(p[k].w&&p[k].h)out[n++]=p[k];return n;}
#endif
