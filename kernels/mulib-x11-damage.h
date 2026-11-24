/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_DAMAGE_H
#define MULIB_X11_DAMAGE_H
#include "mulib-x11-ext.h"
static inline int mulib_x11_damage_create(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t id,uint32_t drawable,uint8_t level){if(!e.present||!id||!drawable||level>3)return 0;unsigned char p[12]={0};mulib_x11_wm_u32(p,id);mulib_x11_wm_u32(p+4,drawable);p[8]=level;return mulib_x11_wm_send(io,e.opcode,1,p,12);}
static inline int mulib_x11_damage_subtract(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t id,uint32_t repair,uint32_t parts){if(!e.present||!id)return 0;unsigned char p[12];mulib_x11_wm_u32(p,id);mulib_x11_wm_u32(p+4,repair);mulib_x11_wm_u32(p+8,parts);return mulib_x11_wm_send(io,e.opcode,3,p,12);}
static inline int mulib_x11_damage_destroy(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t id){unsigned char p[4];if(!e.present||!id)return 0;mulib_x11_wm_u32(p,id);return mulib_x11_wm_send(io,e.opcode,2,p,4);}
#endif
