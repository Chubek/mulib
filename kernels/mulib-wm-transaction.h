/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_TRANSACTION_H
#define MULIB_WM_TRANSACTION_H
#include "mulib-wm-core.h"
typedef struct {uint64_t id,serial;mulib_wm_rect rect;int ack;} mulib_wm_change;
typedef struct {mulib_wm_change *v;size_t n,cap;uint64_t deadline,base_revision;int begun;} mulib_wm_transaction;
static inline int mulib_wm_transaction_begin(mulib_wm_transaction *t,const mulib_wm_core *c,uint64_t deadline){if(!t||!c||t->begun)return 0;t->n=0;t->base_revision=c->revision;t->deadline=deadline;t->begun=1;return 1;}
static inline int mulib_wm_transaction_add(mulib_wm_transaction *t,uint64_t id,uint64_t serial,mulib_wm_rect r){if(!t||!t->begun||!id||!mulib_wm_rect_valid(r)||!r.w||!r.h||t->n==SIZE_MAX)return 0;for(size_t i=0;i<t->n;i++)if(t->v[i].id==id)return 0;if(!mulib_wm_grow((void**)&t->v,&t->cap,t->n+1,sizeof *t->v))return 0;t->v[t->n++]=(mulib_wm_change){id,serial,r,!serial};return 1;}
static inline int mulib_wm_transaction_ack(mulib_wm_transaction *t,uint64_t id,uint64_t serial){if(t&&t->begun)for(size_t i=0;i<t->n;i++)if(t->v[i].id==id&&t->v[i].serial==serial){t->v[i].ack=1;return 1;}return 0;}
/* 1 committed, 0 waiting, -1 stale/timeout. No partial logical mutation. */
static inline int mulib_wm_transaction_commit(mulib_wm_transaction *t,mulib_wm_core *c,uint64_t now){if(!t||!t->begun||!c)return -1;if(c->revision!=t->base_revision||(t->deadline&&now>=t->deadline)){t->begun=0;return -1;}for(size_t i=0;i<t->n;i++){if(!mulib_wm_find(c,t->v[i].id)){t->begun=0;return -1;}if(!t->v[i].ack)return 0;}for(size_t i=0;i<t->n;i++){mulib_wm_window *w=mulib_wm_find(c,t->v[i].id);w->rect=t->v[i].rect;w->configure_serial=t->v[i].serial;}c->revision++;t->begun=0;return 1;}
static inline void mulib_wm_transaction_free(mulib_wm_transaction *t){if(t){free(t->v);memset(t,0,sizeof *t);}}
#endif
