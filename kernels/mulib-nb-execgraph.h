/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_EXECGRAPH_H
#define MULIB_NB_EXECGRAPH_H
#include "mulib-wm-core.h"
typedef struct {uint64_t from,to;} mulib_nb_edge;
typedef struct {uint64_t *ids;size_t n,cap;mulib_nb_edge *edges;size_t ne,ec;} mulib_nb_execgraph;
static inline size_t mulib_nb_execgraph_index(const mulib_nb_execgraph *g,uint64_t id){if(g)for(size_t i=0;i<g->n;i++)if(g->ids[i]==id)return i;return SIZE_MAX;}
static inline int mulib_nb_execgraph_add(mulib_nb_execgraph *g,uint64_t id){if(!g||!id||g->n==SIZE_MAX||mulib_nb_execgraph_index(g,id)!=SIZE_MAX||!mulib_wm_grow((void**)&g->ids,&g->cap,g->n+1,sizeof id))return 0;g->ids[g->n++]=id;return 1;}
/* Kahn topological sort; returns 0 for cycle or allocation failure. */
static inline int mulib_nb_execgraph_sort(const mulib_nb_execgraph *g,uint64_t *out,size_t count){if(!g||count<g->n||(!out&&g->n)||g->n>SIZE_MAX/sizeof(size_t))return 0;size_t *degree=(size_t*)calloc(g->n?g->n:1,sizeof *degree),*queue=(size_t*)malloc((g->n?g->n:1)*sizeof *queue);if(!degree||!queue){free(degree);free(queue);return 0;}for(size_t i=0;i<g->ne;i++){size_t k=mulib_nb_execgraph_index(g,g->edges[i].to);if(k==SIZE_MAX||mulib_nb_execgraph_index(g,g->edges[i].from)==SIZE_MAX){free(degree);free(queue);return 0;}degree[k]++;}size_t a=0,b=0;for(size_t i=0;i<g->n;i++)if(!degree[i])queue[b++]=i;while(a<b){uint64_t id=g->ids[queue[a]];out[a++]=id;for(size_t j=0;j<g->ne;j++)if(g->edges[j].from==id){size_t k=mulib_nb_execgraph_index(g,g->edges[j].to);if(!--degree[k])queue[b++]=k;}}free(degree);free(queue);return a==g->n;}
static inline int mulib_nb_execgraph_link(mulib_nb_execgraph *g,uint64_t from,uint64_t to){if(!g||from==to||g->ne==SIZE_MAX||mulib_nb_execgraph_index(g,from)==SIZE_MAX||mulib_nb_execgraph_index(g,to)==SIZE_MAX||g->n>SIZE_MAX/sizeof(uint64_t))return 0;for(size_t i=0;i<g->ne;i++)if(g->edges[i].from==from&&g->edges[i].to==to)return 1;if(!mulib_wm_grow((void**)&g->edges,&g->ec,g->ne+1,sizeof *g->edges))return 0;uint64_t *v=(uint64_t*)malloc(g->n*sizeof *v);if(!v)return 0;g->edges[g->ne++]=(mulib_nb_edge){from,to};int ok=mulib_nb_execgraph_sort(g,v,g->n);if(!ok)g->ne--;free(v);return ok;}
static inline void mulib_nb_execgraph_free(mulib_nb_execgraph *g){if(g){free(g->ids);free(g->edges);memset(g,0,sizeof *g);}}
#endif
