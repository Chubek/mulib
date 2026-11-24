/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_SUBSURFACE_H
#define MULIB_WL_SUBSURFACE_H
#include "mulib-wl-surface.h"
typedef struct {uint64_t child,parent;int sync;int32_t x,y,pending_x,pending_y;} mulib_wl_subsurface;
typedef struct {mulib_wl_subsurface *v;size_t n,cap;} mulib_wl_subsurfaces;
static inline mulib_wl_subsurface *mulib_wl_subsurface_find(mulib_wl_subsurfaces *s,uint64_t child){if(s)for(size_t i=0;i<s->n;i++)if(s->v[i].child==child)return s->v+i;return NULL;}
static inline int mulib_wl_subsurface_add(mulib_wl_subsurfaces *s,mulib_wl_surface *child,mulib_wl_surface *parent){if(!s||!child||!parent||child==parent||mulib_wl_subsurface_find(s,child->id)||s->n==SIZE_MAX)return 0;for(uint64_t p=parent->id;p;){if(p==child->id)return 0;mulib_wl_subsurface *q=mulib_wl_subsurface_find(s,p);p=q?q->parent:0;}if(!mulib_wm_grow((void**)&s->v,&s->cap,s->n+1,sizeof *s->v)||!mulib_wl_surface_role(child,MULIB_WL_ROLE_SUBSURFACE))return 0;s->v[s->n++]=(mulib_wl_subsurface){.child=child->id,.parent=parent->id,.sync=1};return 1;}
static inline void mulib_wl_subsurface_parent_commit(mulib_wl_subsurfaces *s,uint64_t parent){if(s)for(size_t i=0;i<s->n;i++)if(s->v[i].parent==parent){s->v[i].x=s->v[i].pending_x;s->v[i].y=s->v[i].pending_y;}}
static inline void mulib_wl_subsurfaces_free(mulib_wl_subsurfaces *s){if(s){free(s->v);memset(s,0,sizeof *s);}}
#endif
