/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_COMPOSITE_H
#define MULIB_X11_COMPOSITE_H
#include "mulib-x11-ext.h"
static inline int mulib_x11_composite_redirect(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t w,int subwindows,int manual,int undo){if(!e.present||!w)return 0;unsigned char p[8]={0};mulib_x11_wm_u32(p,w);p[4]=!!manual;return mulib_x11_wm_send(io,e.opcode,(uint8_t)((undo?3:1)+!!subwindows),p,8);}
static inline int mulib_x11_composite_name_pixmap(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t w,uint32_t pixmap){if(!e.present||!w||!pixmap)return 0;unsigned char p[8];mulib_x11_wm_u32(p,w);mulib_x11_wm_u32(p+4,pixmap);return mulib_x11_wm_send(io,e.opcode,6,p,8);}
#endif
