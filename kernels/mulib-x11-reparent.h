/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_REPARENT_H
#define MULIB_X11_REPARENT_H
#include "mulib-x11-ext.h"
static inline int mulib_x11_reparent(mulib_x11_wm_io *io,uint32_t win,uint32_t parent,int16_t x,int16_t y){if(!win||!parent||win==parent)return 0;unsigned char p[12];mulib_x11_wm_u32(p,win);mulib_x11_wm_u32(p+4,parent);mulib_x11_wm_u16(p+8,(uint16_t)x);mulib_x11_wm_u16(p+10,(uint16_t)y);return mulib_x11_wm_send(io,7,0,p,12);}
static inline int mulib_x11_reparent_save_set(mulib_x11_wm_io *io,uint32_t win,int remove){unsigned char p[4];if(!win)return 0;mulib_x11_wm_u32(p,win);return mulib_x11_wm_send(io,6,!!remove,p,4);}
static inline int mulib_x11_reparent_select(mulib_x11_wm_io *io,uint32_t root,uint32_t mask){unsigned char p[12];if(!root)return 0;mulib_x11_wm_u32(p,root);mulib_x11_wm_u32(p+4,1u<<11);mulib_x11_wm_u32(p+8,mask);return mulib_x11_wm_send(io,2,0,p,12);}
static inline int mulib_x11_reparent_configure(mulib_x11_wm_io *io,uint32_t win,uint16_t mask,const uint32_t *v,size_t count){if(!win||mask&~127u||(!v&&count)||count>7)return 0;size_t expected=0;for(unsigned i=0;i<7;i++)expected+=(mask>>i)&1u;if(expected!=count)return 0;unsigned char p[36]={0};mulib_x11_wm_u32(p,win);mulib_x11_wm_u16(p+4,mask);for(size_t i=0;i<count;i++)mulib_x11_wm_u32(p+8+4*i,v[i]);return mulib_x11_wm_send(io,12,0,p,8+4*count);}
#endif
