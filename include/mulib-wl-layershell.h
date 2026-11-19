/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_LAYERSHELL_H
#define MULIB_WL_LAYERSHELL_H
#include "mulib-wm-core.h"
enum {MULIB_WL_ANCHOR_TOP=1,MULIB_WL_ANCHOR_BOTTOM=2,MULIB_WL_ANCHOR_LEFT=4,MULIB_WL_ANCHOR_RIGHT=8};
/* Exclusive zones on a single anchored edge (with optional opposing orthogonal anchors). */
static inline int mulib_wl_layershell_reserve(mulib_wm_rect *area,unsigned anchor,int zone,int margin){if(!area||!mulib_wm_rect_valid(*area)||anchor&~15u||zone<0)return 0;int64_t z=(int64_t)zone+margin;if(z<0)z=0;unsigned edge=0;if(anchor==1||anchor==13)edge=1;else if(anchor==2||anchor==14)edge=2;else if(anchor==4||anchor==7)edge=4;else if(anchor==8||anchor==11)edge=8;else return zone==0;if(!zone)return 1;if(edge<=2){int a=z>area->h?area->h:(int)z;if(edge==1)area->y+=a;area->h-=a;}else{int a=z>area->w?area->w:(int)z;if(edge==4)area->x+=a;area->w-=a;}return 1;}
#endif
