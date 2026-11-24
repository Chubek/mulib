/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_HISTORY_H
#define MULIB_NB_HISTORY_H
#include "mulib-nb-core.h"
typedef struct {mulib_nb *v;size_t n,cap,cursor,limit;} mulib_nb_history;
static inline int mulib_nb_history_record(mulib_nb_history *h,const mulib_nb *n){if(!h||!h->limit||!n)return 0;mulib_nb c={0};if(!mulib_nb_clone(n,&c))return 0;size_t keep=h->n?h->cursor+1:0;if(keep==SIZE_MAX||!mulib_wm_grow((void**)&h->v,&h->cap,keep+1,sizeof c)){mulib_nb_free(&c);return 0;}for(size_t i=keep;i<h->n;i++)mulib_nb_free(h->v+i);h->n=keep;if(h->n>=h->limit){mulib_nb_free(h->v);memmove(h->v,h->v+1,(--h->n)*sizeof c);}h->v[h->n++]=c;h->cursor=h->n-1;return 1;}
static inline int mulib_nb_history_step(mulib_nb_history *h,mulib_nb *n,int redo){if(!h||!n||!h->n||(redo?h->cursor+1>=h->n:h->cursor==0))return 0;size_t k=redo?h->cursor+1:h->cursor-1;mulib_nb t={0};if(!mulib_nb_clone(h->v+k,&t))return 0;if(t.next<n->next)t.next=n->next;t.revision=n->revision+1;for(size_t i=0;i<t.n;i++){t.v[i].window=0;t.v[i].flags&=~MULIB_NB_TRUSTED;mulib_nb_cell *old=mulib_nb_find(n,t.v[i].id);if(old&&t.v[i].revision<=old->revision)t.v[i].revision=old->revision==UINT64_MAX?UINT64_MAX:old->revision+1;}mulib_nb_free(n);*n=t;h->cursor=k;return 1;}
static inline void mulib_nb_history_free(mulib_nb_history *h){if(h){for(size_t i=0;i<h->n;i++)mulib_nb_free(h->v+i);free(h->v);memset(h,0,sizeof *h);}}
#endif
