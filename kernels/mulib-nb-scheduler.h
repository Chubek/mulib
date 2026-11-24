/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_SCHEDULER_H
#define MULIB_NB_SCHEDULER_H
#include "mulib-nb-execgraph.h"
enum {MULIB_NB_JOB_WAIT=0,MULIB_NB_JOB_ACTIVE,MULIB_NB_JOB_OK,MULIB_NB_JOB_ERROR,MULIB_NB_JOB_BLOCKED};
typedef struct {const mulib_nb_execgraph *graph;unsigned char *state;size_t n,active,parallel;} mulib_nb_scheduler;
/* Graph borrowed and immutable throughout execution. */
static inline int mulib_nb_scheduler_init(mulib_nb_scheduler *s,const mulib_nb_execgraph *g,size_t parallel){if(!s||!g||!parallel||g->n>SIZE_MAX/sizeof(uint64_t))return 0;uint64_t *v=(uint64_t*)malloc((g->n?g->n:1)*sizeof *v);if(!v)return 0;int ok=mulib_nb_execgraph_sort(g,v,g->n);free(v);if(!ok)return 0;unsigned char *p=(unsigned char*)calloc(g->n?g->n:1,1);if(!p)return 0;*s=(mulib_nb_scheduler){g,p,g->n,0,parallel};return 1;}
static inline uint64_t mulib_nb_scheduler_next(mulib_nb_scheduler *s){if(!s||!s->graph||s->active>=s->parallel)return 0;const mulib_nb_execgraph *g=s->graph;for(size_t pass=0;pass<=s->n;pass++){int changed=0;for(size_t i=0;i<s->n;i++)if(s->state[i]==MULIB_NB_JOB_WAIT){int ready=1,blocked=0;for(size_t j=0;j<g->ne;j++)if(g->edges[j].to==g->ids[i]){size_t k=mulib_nb_execgraph_index(g,g->edges[j].from);if(k>=s->n)return 0;unsigned char v=s->state[k];if(v==MULIB_NB_JOB_ERROR||v==MULIB_NB_JOB_BLOCKED)blocked=1;if(v!=MULIB_NB_JOB_OK)ready=0;}if(blocked){s->state[i]=MULIB_NB_JOB_BLOCKED;changed=1;}else if(ready){s->state[i]=MULIB_NB_JOB_ACTIVE;s->active++;return g->ids[i];}}if(!changed)break;}return 0;}
static inline int mulib_nb_scheduler_done(mulib_nb_scheduler *s,uint64_t id,int success){if(!s)return 0;size_t k=mulib_nb_execgraph_index(s->graph,id);if(k>=s->n||s->state[k]!=MULIB_NB_JOB_ACTIVE)return 0;s->state[k]=success?MULIB_NB_JOB_OK:MULIB_NB_JOB_ERROR;s->active--;return 1;}
static inline void mulib_nb_scheduler_free(mulib_nb_scheduler *s){if(s){free(s->state);memset(s,0,sizeof *s);}}
#endif
