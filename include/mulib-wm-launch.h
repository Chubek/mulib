/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_LAUNCH_H
#define MULIB_WM_LAUNCH_H
#include "mulib-wm-core.h"
typedef struct {uint64_t token,workspace,cell,deadline;long pid;char *app;} mulib_wm_launch_record;
typedef struct {mulib_wm_launch_record *v;size_t n,cap;uint64_t next;} mulib_wm_launches;
static inline uint64_t mulib_wm_launch_track(mulib_wm_launches *s,long pid,const char *app,uint64_t ws,uint64_t cell,uint64_t deadline){if(!s||s->next==UINT64_MAX||s->n==SIZE_MAX||(!app&&pid<=0))return 0;char *a=mulib_wm_strdup(app?app:"");if(!a||!mulib_wm_grow((void**)&s->v,&s->cap,s->n+1,sizeof *s->v)){free(a);return 0;}uint64_t token=++s->next;s->v[s->n++]=(mulib_wm_launch_record){token,ws,cell,deadline,pid,a};return token;}
/* Token must come from trusted activation plumbing; PID/app heuristics are advisory. */
static inline int mulib_wm_launch_match(mulib_wm_launches *s,mulib_wm_window *w,uint64_t token,long pid,uint64_t now){if(!s||!w)return 0;size_t found=SIZE_MAX;for(size_t i=0;i<s->n;i++){mulib_wm_launch_record *r=s->v+i;if((r->deadline&&now>=r->deadline))continue;if(token?r->token==token:((pid>0&&r->pid==pid)||(*r->app&&!strcmp(r->app,w->app_id)))){if(found!=SIZE_MAX)return 0;found=i;}}if(found==SIZE_MAX)return 0;mulib_wm_launch_record *r=s->v+found;w->workspace=r->workspace;w->cell=r->cell;free(r->app);memmove(r,r+1,(s->n-found-1)*sizeof *r);s->n--;return 1;}
static inline void mulib_wm_launches_free(mulib_wm_launches *s){if(s){for(size_t i=0;i<s->n;i++)free(s->v[i].app);free(s->v);memset(s,0,sizeof *s);}}
#endif
