/* MuLIB compiler tools: Dependency graph with deterministic topological sort and cycle detection.
 * ISO C11 single-header module. Define MULIB_IMPL_IMPORTGRAPH in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_IMPORTGRAPH_H
#define MULIB_IMPORTGRAPH_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t from,to;
} mulib_import_edge_t;
typedef struct {mulib_import_edge_t *edges;
size_t count,capacity,nodes;
} mulib_importgraph_t;
int mulib_importgraph_add(mulib_importgraph_t *g,size_t dependency,size_t consumer);
/* 1 sorted; 0 cycle or invalid; output n entries and does not change on failure. */
int mulib_importgraph_order(const mulib_importgraph_t *g,size_t *out,size_t capacity);
void mulib_importgraph_free(mulib_importgraph_t *g);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPORTGRAPH_H */
#if defined(MULIB_IMPL_IMPORTGRAPH) && !defined(MULIB_IMPORTGRAPH_IMPLEMENTED)
#define MULIB_IMPORTGRAPH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_importgraph_add(mulib_importgraph_t *g,size_t dep,size_t consumer){if(!g||dep>=g->nodes||consumer>=g->nodes)return 0;
for(size_t i=0;i<g->count;i++)if(g->edges[i].from==dep&&g->edges[i].to==consumer)return 1;
if(g->count==g->capacity){size_t cap=g->capacity?g->capacity*2:8;
if(cap<g->capacity||cap>SIZE_MAX/sizeof(*g->edges))return 0;
void *p=realloc(g->edges,cap*sizeof(*g->edges));
if(!p)return 0;
g->edges=(mulib_import_edge_t*)p;
g->capacity=cap;
}g->edges[g->count++]=(mulib_import_edge_t){dep,consumer};
return 1;
}
int mulib_importgraph_order(const mulib_importgraph_t *g,size_t *out,size_t cap){if(!g||(!out&&g->nodes)||cap<g->nodes||g->nodes>SIZE_MAX/sizeof(size_t))return 0;
size_t *degree=(size_t*)calloc(g->nodes?g->nodes:1,sizeof(size_t));
size_t *tmp=(size_t*)malloc((g->nodes?g->nodes:1)*sizeof(size_t));
if(!degree||!tmp){free(degree);
free(tmp);
return 0;
}for(size_t i=0;i<g->count;i++){if(g->edges[i].from>=g->nodes||g->edges[i].to>=g->nodes||degree[g->edges[i].to]==SIZE_MAX){free(degree);
free(tmp);
return 0;
}degree[g->edges[i].to]++;
}size_t k=0;
for(size_t step=0;step<g->nodes;step++){size_t v=0;
for(;v<g->nodes;v++)if(degree[v]==0)break;
if(v==g->nodes){free(degree);
free(tmp);
return 0;
}degree[v]=SIZE_MAX;
tmp[k++]=v;
for(size_t i=0;i<g->count;i++)if(g->edges[i].from==v)degree[g->edges[i].to]--;
 }if(k)memcpy(out,tmp,k*sizeof(size_t));
free(degree);
free(tmp);
return 1;
}
void mulib_importgraph_free(mulib_importgraph_t *g){if(g){free(g->edges);
memset(g,0,sizeof(*g));
}}
#ifdef __cplusplus
}
#endif
#endif
