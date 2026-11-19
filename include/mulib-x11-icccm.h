/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_ICCCM_H
#define MULIB_X11_ICCCM_H
#include "mulib-x11-property.h"
typedef struct {uint32_t flags,min_w,min_h,max_w,max_h,inc_w,inc_h,base_w,base_h;} mulib_x11_size_hints;
static inline int mulib_x11_icccm_size_hints(const mulib_x11_property *p,mulib_x11_size_hints *h){if(!p||!h||p->format!=32||p->items<15||p->bytes<60)return 0;const unsigned char *d=p->data;*h=(mulib_x11_size_hints){mulib_x11_wm_r32(d),mulib_x11_wm_r32(d+20),mulib_x11_wm_r32(d+24),mulib_x11_wm_r32(d+28),mulib_x11_wm_r32(d+32),mulib_x11_wm_r32(d+36),mulib_x11_wm_r32(d+40),p->items>=18&&p->bytes>=72?mulib_x11_wm_r32(d+60):0,p->items>=18&&p->bytes>=72?mulib_x11_wm_r32(d+64):0};return 1;}
static inline int mulib_x11_icccm_protocol(mulib_x11_wm_io *io,uint32_t win,uint32_t protocols,uint32_t protocol,uint32_t time){if(!win||!protocols||!protocol)return 0;unsigned char p[40]={0};mulib_x11_wm_u32(p,win);p[8]=33;p[9]=32;mulib_x11_wm_u32(p+12,win);mulib_x11_wm_u32(p+16,protocols);mulib_x11_wm_u32(p+20,protocol);mulib_x11_wm_u32(p+24,time);return mulib_x11_wm_send(io,25,0,p,sizeof p);}
#endif
