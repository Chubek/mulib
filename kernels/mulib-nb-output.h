/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_OUTPUT_H
#define MULIB_NB_OUTPUT_H
#include "mulib-wm-core.h"
typedef struct {uint64_t cell,display;char *mime;unsigned char *data;size_t n;} mulib_nb_output;
typedef struct {mulib_nb_output *v;size_t n,cap,bytes,limit;} mulib_nb_outputs;
static inline int mulib_nb_output_add(mulib_nb_outputs *o,uint64_t cell,uint64_t display,const char *mime,const void *data,size_t n){if(!o||!cell||!mime||!*mime||(!data&&n)||o->bytes>o->limit||n>o->limit-o->bytes||o->n==SIZE_MAX)return 0;char *m=mulib_wm_strdup(mime);unsigned char *p=(unsigned char*)malloc(n?n:1);if(!m||!p||!mulib_wm_grow((void**)&o->v,&o->cap,o->n+1,sizeof *o->v)){free(m);free(p);return 0;}if(n)memcpy(p,data,n);o->v[o->n++]=(mulib_nb_output){cell,display,m,p,n};o->bytes+=n;return 1;}
static inline void mulib_nb_output_clear(mulib_nb_outputs *o,uint64_t cell){if(!o)return;size_t k=0;for(size_t i=0;i<o->n;i++){if(!cell||o->v[i].cell==cell){o->bytes-=o->v[i].n;free(o->v[i].mime);free(o->v[i].data);}else o->v[k++]=o->v[i];}o->n=k;}
static inline void mulib_nb_outputs_free(mulib_nb_outputs *o){if(o){mulib_nb_output_clear(o,0);free(o->v);memset(o,0,sizeof *o);}}
#endif
