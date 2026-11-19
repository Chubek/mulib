/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_CONTAINER_H
#define MULIB_WM_CONTAINER_H
#include "mulib-wm-core.h"
typedef struct {uint64_t id,parent,window;unsigned kind;} mulib_wm_container;
typedef struct {mulib_wm_container *v;size_t n,cap;uint64_t next;} mulib_wm_containers;
static inline mulib_wm_container *mulib_wm_container_find(mulib_wm_containers *s,uint64_t id){if(s)for(size_t i=0;i<s->n;i++)if(s->v[i].id==id)return s->v+i;return NULL;}
static inline uint64_t mulib_wm_container_add(mulib_wm_containers *s,uint64_t parent,uint64_t win,unsigned kind){if(!s||s->next==UINT64_MAX||s->n==SIZE_MAX||(parent&&!mulib_wm_container_find(s,parent)))return 0;if(win)for(size_t i=0;i<s->n;i++)if(s->v[i].window==win)return 0;if(!mulib_wm_grow((void**)&s->v,&s->cap,s->n+1,sizeof *s->v))return 0;uint64_t id=++s->next;s->v[s->n++]=(mulib_wm_container){id,parent,win,kind};return id;}
static inline int mulib_wm_container_reparent(mulib_wm_containers *s,uint64_t id,uint64_t parent){mulib_wm_container *v=mulib_wm_container_find(s,id);if(!v)return 0;for(uint64_t p=parent;p;){mulib_wm_container *q=mulib_wm_container_find(s,p);if(!q||p==id)return 0;p=q->parent;}v->parent=parent;return 1;}
static inline int mulib_wm_container_remove(mulib_wm_containers *s,uint64_t id){mulib_wm_container *v=mulib_wm_container_find(s,id);if(!v)return 0;size_t k=(size_t)(v-s->v);for(size_t i=0;i<s->n;i++)if(s->v[i].parent==id)s->v[i].parent=v->parent;memmove(v,v+1,(s->n-k-1)*sizeof *v);s->n--;return 1;}
static inline void mulib_wm_containers_free(mulib_wm_containers *s){if(s){free(s->v);memset(s,0,sizeof *s);}}
#endif
