/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_XINPUT_H
#define MULIB_X11_XINPUT_H
#include "mulib-x11-ext.h"
static inline int mulib_x11_xinput_select(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t w,uint16_t device,const unsigned char *mask,size_t n){if(!e.present||!w||!mask||!n||n%4||n>128)return 0;unsigned char p[140]={0};mulib_x11_wm_u32(p,w);mulib_x11_wm_u16(p+4,1);mulib_x11_wm_u16(p+8,device);mulib_x11_wm_u16(p+10,(uint16_t)(n/4));memcpy(p+12,mask,n);return mulib_x11_wm_send(io,e.opcode,46,p,12+n);}
static inline int mulib_x11_xinput_version(mulib_x11_wm_io *io,mulib_x11_extension e,uint16_t major,uint16_t minor,uint16_t *a,uint16_t *b){if(!e.present||!a||!b)return 0;unsigned char p[4],*r;size_t n;mulib_x11_wm_u16(p,major);mulib_x11_wm_u16(p+2,minor);if(!mulib_x11_wm_reply(io,e.opcode,47,p,4,&r,&n))return 0;*a=mulib_x11_wm_r16(r+8);*b=mulib_x11_wm_r16(r+10);free(r);return 1;}
#endif
