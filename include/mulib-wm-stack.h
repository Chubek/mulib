/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_STACK_H
#define MULIB_WM_STACK_H
#include "mulib-wm-core.h"
/* Registry order is bottom-to-top; pointers into it are invalidated by moves. */
static inline int mulib_wm_stack_raise(mulib_wm_core *c,uint64_t id,int top){mulib_wm_window *w=mulib_wm_find(c,id);if(!w)return 0;size_t i=(size_t)(w-c->v);mulib_wm_window v=*w;if(top){memmove(w,w+1,(c->n-i-1)*sizeof *w);c->v[c->n-1]=v;}else{memmove(c->v+1,c->v,i*sizeof *w);c->v[0]=v;}c->revision++;return 1;}
static inline uint64_t mulib_wm_stack_at(const mulib_wm_core *c,uint64_t ws,double x,double y){if(c)for(size_t i=c->n;i;i--){const mulib_wm_window *w=c->v+i-1;if(w->workspace==ws&&(w->flags&MULIB_WM_MAPPED)&&!(w->flags&MULIB_WM_MINIMIZED)&&mulib_wm_contains(w->rect,x,y))return w->id;}return 0;}
#endif
