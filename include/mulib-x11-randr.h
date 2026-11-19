/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_RANDR_H
#define MULIB_X11_RANDR_H
#include "mulib-x11-ext.h"
static inline int mulib_x11_randr_select(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t w,uint16_t mask){unsigned char p[8]={0};if(!e.present||!w)return 0;mulib_x11_wm_u32(p,w);mulib_x11_wm_u16(p+4,mask);return mulib_x11_wm_send(io,e.opcode,4,p,8);}
/* Raw GetScreenResourcesCurrent reply, caller owns buffer; validate arrays before use. */
static inline int mulib_x11_randr_resources(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t root,unsigned char **reply,size_t *n){if(!e.present||!root)return 0;unsigned char p[4],*r;size_t z;mulib_x11_wm_u32(p,root);if(!mulib_x11_wm_reply(io,e.opcode,25,p,4,&r,&z))return 0;size_t need=(size_t)mulib_x11_wm_r16(r+16)*4+(size_t)mulib_x11_wm_r16(r+18)*4+(size_t)mulib_x11_wm_r16(r+20)*32+mulib_x11_wm_r16(r+22);if(need>z-32||!reply||!n){free(r);return 0;}*reply=r;*n=z;return 1;}
#endif
