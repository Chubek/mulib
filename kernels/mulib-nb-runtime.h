/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_RUNTIME_H
#define MULIB_NB_RUNTIME_H
#include "mulib-nb-message.h"
typedef int (*mulib_nb_emit_fn)(void *,const mulib_nb_message *);
typedef struct {const char *language;void *user;int (*execute)(void *,uint64_t,uint64_t,const char *,mulib_nb_emit_fn,void *);int (*interrupt)(void *,uint64_t);} mulib_nb_runtime_ops;
typedef struct {mulib_nb_runtime_ops *v;size_t n,cap;} mulib_nb_runtimes;
static inline int mulib_nb_runtime_register(mulib_nb_runtimes *r,mulib_nb_runtime_ops op){if(!r||!op.language||!*op.language||!op.execute||r->n==SIZE_MAX)return 0;for(size_t i=0;i<r->n;i++)if(!strcmp(r->v[i].language,op.language))return 0;if(!mulib_wm_grow((void**)&r->v,&r->cap,r->n+1,sizeof op))return 0;r->v[r->n++]=op;return 1;}
static inline const mulib_nb_runtime_ops *mulib_nb_runtime_find(const mulib_nb_runtimes *r,const char *lang){if(r&&lang)for(size_t i=0;i<r->n;i++)if(!strcmp(r->v[i].language,lang))return r->v+i;return NULL;}
static inline void mulib_nb_runtimes_free(mulib_nb_runtimes *r){if(r){free(r->v);memset(r,0,sizeof *r);}}
#endif
