/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_GRAB_H
#define MULIB_X11_GRAB_H
#include "mulib-x11-ext.h"
static inline int mulib_x11_grab_key(mulib_x11_wm_io *io,uint32_t root,uint8_t key,uint16_t mods,int owner){if(!root)return 0;unsigned char p[12]={0};mulib_x11_wm_u32(p,root);mulib_x11_wm_u16(p+4,mods);p[6]=key;p[7]=1;p[8]=1;return mulib_x11_wm_send(io,33,!!owner,p,12);}
static inline int mulib_x11_ungrab_key(mulib_x11_wm_io *io,uint32_t root,uint8_t key,uint16_t mods){if(!root)return 0;unsigned char p[8]={0};mulib_x11_wm_u32(p,root);mulib_x11_wm_u16(p+4,mods);return mulib_x11_wm_send(io,34,key,p,8);}
static inline int mulib_x11_grab_pointer(mulib_x11_wm_io *io,uint32_t root,uint16_t mask,uint32_t time,uint8_t *status){if(!root||!status)return 0;unsigned char p[20]={0},*r;size_t n;mulib_x11_wm_u32(p,root);mulib_x11_wm_u16(p+4,mask);p[6]=p[7]=1;mulib_x11_wm_u32(p+16,time);if(!mulib_x11_wm_reply(io,26,0,p,20,&r,&n))return 0;*status=r[1];free(r);return 1;}
#endif
