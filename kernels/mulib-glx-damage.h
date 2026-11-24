/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_GLX_DAMAGE_H
#define MULIB_GLX_DAMAGE_H
#include "mulib-wm-core.h"
typedef struct {mulib_wm_rect *v;size_t n,cap,limit;mulib_wm_rect bounds;int full;} mulib_glx_damage;
static inline void mulib_glx_damage_add(mulib_glx_damage *d,mulib_wm_rect r){if(!d||d->full)return;r=mulib_wm_intersect(d->bounds,r);if(!r.w||!r.h)return;if(d->n>=d->limit||d->n==SIZE_MAX||!mulib_wm_grow((void**)&d->v,&d->cap,d->n+1,sizeof r)){d->full=1;d->n=0;return;}d->v[d->n++]=r;}
static inline void mulib_glx_damage_move(mulib_glx_damage *d,mulib_wm_rect old,mulib_wm_rect next){mulib_glx_damage_add(d,old);mulib_glx_damage_add(d,next);}
static inline void mulib_glx_damage_reset(mulib_glx_damage *d){if(d){d->n=0;d->full=0;}}
static inline void mulib_glx_damage_free(mulib_glx_damage *d){if(d){free(d->v);memset(d,0,sizeof *d);}}
#endif
