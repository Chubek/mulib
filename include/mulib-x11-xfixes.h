/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_XFIXES_H
#define MULIB_X11_XFIXES_H
#include "mulib-x11-ext.h"
typedef struct {int16_t x,y;uint16_t w,h;} mulib_x11_xfixes_rect;
static inline int mulib_x11_xfixes_region(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t id,const mulib_x11_xfixes_rect *r,size_t n){if(!e.present||!id||(!r&&n)||n>32760)return 0;unsigned char *p=(unsigned char*)malloc(4+n*8);if(!p)return 0;mulib_x11_wm_u32(p,id);for(size_t i=0;i<n;i++){unsigned char *q=p+4+i*8;mulib_x11_wm_u16(q,(uint16_t)r[i].x);mulib_x11_wm_u16(q+2,(uint16_t)r[i].y);mulib_x11_wm_u16(q+4,r[i].w);mulib_x11_wm_u16(q+6,r[i].h);}int ok=mulib_x11_wm_send(io,e.opcode,5,p,4+8*n);free(p);return ok;}
static inline int mulib_x11_xfixes_selection(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t w,uint32_t atom,uint32_t mask){if(!e.present||!w||!atom)return 0;unsigned char p[12];mulib_x11_wm_u32(p,w);mulib_x11_wm_u32(p+4,atom);mulib_x11_wm_u32(p+8,mask);return mulib_x11_wm_send(io,e.opcode,2,p,12);}
#endif
