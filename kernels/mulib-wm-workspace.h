/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_WORKSPACE_H
#define MULIB_WM_WORKSPACE_H
#include "mulib-wm-core.h"
typedef struct {uint64_t id;char *name;} mulib_wm_workspace;
typedef struct {mulib_wm_workspace *v;size_t n,cap;uint64_t next,active;} mulib_wm_workspaces;
static inline uint64_t mulib_wm_workspace_add(mulib_wm_workspaces *s,const char *name){if(!s||!name||!*name||s->next==UINT64_MAX||s->n==SIZE_MAX)return 0;char *p=mulib_wm_strdup(name);if(!p||!mulib_wm_grow((void**)&s->v,&s->cap,s->n+1,sizeof *s->v)){free(p);return 0;}uint64_t id=++s->next;s->v[s->n++]=(mulib_wm_workspace){id,p};if(!s->active)s->active=id;return id;}
static inline int mulib_wm_workspace_select(mulib_wm_workspaces *s,uint64_t id){if(s)for(size_t i=0;i<s->n;i++)if(s->v[i].id==id){s->active=id;return 1;}return 0;}
static inline int mulib_wm_workspace_move(mulib_wm_workspaces *s,mulib_wm_core *c,uint64_t win,uint64_t dest){mulib_wm_window *w=mulib_wm_find(c,win);if(!s||!w)return 0;for(size_t i=0;i<s->n;i++)if(s->v[i].id==dest){w->workspace=dest;c->revision++;return 1;}return 0;}
static inline void mulib_wm_workspaces_free(mulib_wm_workspaces *s){if(s){for(size_t i=0;i<s->n;i++)free(s->v[i].name);free(s->v);memset(s,0,sizeof *s);}}
#endif
