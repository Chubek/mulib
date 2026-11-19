/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_LAYOUT_H
#define MULIB_WM_LAYOUT_H
#include "mulib-wm-core.h"
typedef enum {MULIB_WM_LAYOUT_VERTICAL,MULIB_WM_LAYOUT_HORIZONTAL,MULIB_WM_LAYOUT_GRID,MULIB_WM_LAYOUT_MONOCLE,MULIB_WM_LAYOUT_MASTER} mulib_wm_layout_kind;
static inline int mulib_wm_layout(mulib_wm_layout_kind k,mulib_wm_rect area,size_t n,int gap,mulib_wm_rect *out){if(!mulib_wm_rect_valid(area)||gap<0||(!out&&n)||n>INT32_MAX||k<MULIB_WM_LAYOUT_VERTICAL||k>MULIB_WM_LAYOUT_MASTER)return 0;if(!n)return 1;if(k==MULIB_WM_LAYOUT_MONOCLE){for(size_t i=0;i<n;i++)out[i]=area;return 1;}if(k==MULIB_WM_LAYOUT_MASTER&&n>1){if(area.w<gap)return 0;int w=(area.w-gap)/2;mulib_wm_rect rest={area.x+w+gap,area.y,area.w-w-gap,area.h};if(!mulib_wm_layout(MULIB_WM_LAYOUT_VERTICAL,rest,n-1,gap,out+1))return 0;out[0]=(mulib_wm_rect){area.x,area.y,w,area.h};return 1;}size_t cols=k==MULIB_WM_LAYOUT_HORIZONTAL?n:1;if(k==MULIB_WM_LAYOUT_GRID){while(cols*cols<n)cols++;}size_t rows=(n+cols-1)/cols;int64_t aw=area.w-(int64_t)(cols-1)*gap,ah=area.h-(int64_t)(rows-1)*gap;if(aw<0||ah<0)return 0;for(size_t i=0;i<n;i++){int64_t x=(int64_t)(i%cols),y=(int64_t)(i/cols);int64_t x0=x*aw/(int64_t)cols,x1=(x+1)*aw/(int64_t)cols,y0=y*ah/(int64_t)rows,y1=(y+1)*ah/(int64_t)rows;out[i]=(mulib_wm_rect){(int32_t)(area.x+x0+x*gap),(int32_t)(area.y+y0+y*gap),(int32_t)(x1-x0),(int32_t)(y1-y0)};}return 1;}
#endif
