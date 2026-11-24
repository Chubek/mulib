/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_FOCUS_H
#define MULIB_WM_FOCUS_H
#include "mulib-wm-core.h"
static inline int mulib_wm_focusable(const mulib_wm_window *w,uint64_t workspace){return w&&(w->flags&MULIB_WM_MAPPED)&&!(w->flags&MULIB_WM_MINIMIZED)&&(!workspace||w->workspace==workspace);}
static inline int mulib_wm_focus(mulib_wm_core *c,uint64_t id,uint64_t workspace){if(!c)return 0;if(id&&!mulib_wm_focusable(mulib_wm_find(c,id),workspace))return 0;c->focused=id;c->revision++;return 1;}
static inline uint64_t mulib_wm_focus_cycle(mulib_wm_core *c,uint64_t ws,int backwards){if(!c||!c->n)return 0;size_t start=backwards?0:c->n-1;for(size_t i=0;i<c->n;i++)if(c->v[i].id==c->focused)start=i;for(size_t j=0;j<c->n;j++){start=backwards?(start?start-1:c->n-1):(start+1)%c->n;if(mulib_wm_focusable(c->v+start,ws)){mulib_wm_focus(c,c->v[start].id,ws);return c->focused;}}return 0;}
#endif
