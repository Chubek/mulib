/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_METADATA_H
#define MULIB_NB_METADATA_H
#include "mulib-wm-core.h"
typedef struct {uint64_t cell;char *key,*value;} mulib_nb_meta;
typedef struct {mulib_nb_meta *v;size_t n,cap;} mulib_nb_metadata;
static inline const char *mulib_nb_metadata_get(const mulib_nb_metadata *m,uint64_t cell,const char *key){if(m&&key)for(size_t i=0;i<m->n;i++)if(m->v[i].cell==cell&&!strcmp(m->v[i].key,key))return m->v[i].value;return NULL;}
static inline int mulib_nb_metadata_set(mulib_nb_metadata *m,uint64_t cell,const char *key,const char *value){if(!m||!key||!*key||!value)return 0;char *v=mulib_wm_strdup(value);if(!v)return 0;for(size_t i=0;i<m->n;i++)if(m->v[i].cell==cell&&!strcmp(m->v[i].key,key)){free(m->v[i].value);m->v[i].value=v;return 1;}char *k=mulib_wm_strdup(key);if(!k||m->n==SIZE_MAX||!mulib_wm_grow((void**)&m->v,&m->cap,m->n+1,sizeof *m->v)){free(k);free(v);return 0;}m->v[m->n++]=(mulib_nb_meta){cell,k,v};return 1;}
static inline void mulib_nb_metadata_free(mulib_nb_metadata *m){if(m){for(size_t i=0;i<m->n;i++){free(m->v[i].key);free(m->v[i].value);}free(m->v);memset(m,0,sizeof *m);}}
#endif
