/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_EWMH_H
#define MULIB_X11_EWMH_H
#include "mulib-x11-property.h"
static inline int mulib_x11_ewmh_cardinals(mulib_x11_wm_io *io,uint32_t root,uint32_t property,uint32_t type,const uint32_t *items,size_t n){if(n>65525||(!items&&n))return 0;unsigned char *p=n?(unsigned char*)malloc(n*4):NULL;if(n&&!p)return 0;for(size_t i=0;i<n;i++)mulib_x11_wm_u32(p+4*i,items[i]);int ok=mulib_x11_property_set(io,root,property,type,32,p,n*4);free(p);return ok;}
typedef struct {uint32_t window,type,data[5];} mulib_x11_ewmh_message;
static inline int mulib_x11_ewmh_decode(const unsigned char *p,size_t n,mulib_x11_ewmh_message *out){if(!p||n<32||!out||(p[0]&127)!=33||p[1]!=32)return 0;out->window=mulib_x11_wm_r32(p+4);out->type=mulib_x11_wm_r32(p+8);for(size_t i=0;i<5;i++)out->data[i]=mulib_x11_wm_r32(p+12+i*4);return 1;}
#endif
