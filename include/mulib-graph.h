/* MuLIB compiler tools: Patched dense directed graph with BFS, deterministic topological ordering and reversed edges.
 * ISO C11 single-header module. Define MULIB_IMPL_GRAPH in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_GRAPH_H
#define MULIB_GRAPH_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_graph {unsigned char *edges;
size_t n;
} mulib_graph_t;
int mulib_graph_init(mulib_graph_t *g,size_t vertices);
int mulib_graph_edge(mulib_graph_t *g,size_t src,size_t dst,int enabled);
int mulib_graph_bfs(const mulib_graph_t *g,size_t start,size_t *order,size_t capacity);
void mulib_graph_free(mulib_graph_t *g);
/* Returns 1 for DAG; 0 for cycles/invalid/OOM; output unchanged on failure. */
int mulib_graph_toposort(const mulib_graph_t *g,size_t *order,size_t capacity);
/* Creates deep-copy reversal; out must be zero initialized. */
int mulib_graph_reverse(const mulib_graph_t *g,mulib_graph_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GRAPH_H */
#if defined(MULIB_IMPL_GRAPH) && !defined(MULIB_GRAPH_IMPLEMENTED)
#define MULIB_GRAPH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_graph_init(mulib_graph_t *g,size_t n){if(!g||(n&&n>SIZE_MAX/n))return 0;
unsigned char *p=(unsigned char*)calloc(n?n*n:1,1);
if(!p)return 0;
g->n=n;
g->edges=p;
return 1;
}
int mulib_graph_edge(mulib_graph_t *g,size_t a,size_t b,int on){if(!g||!g->edges||a>=g->n||b>=g->n)return 0;
g->edges[a*g->n+b]=(unsigned char)!!on;
return 1;
}
int mulib_graph_bfs(const mulib_graph_t *g,size_t start,size_t *out,size_t cap){if(!g||!g->edges||start>=g->n||!out||cap<g->n||g->n>SIZE_MAX/sizeof(size_t))return -1;
 unsigned char *seen=(unsigned char*)calloc(g->n,1);
size_t *q=(size_t*)malloc(g->n*sizeof(size_t));
if(!seen||!q){free(q);
free(seen);
return -1;
}
 size_t head=0,tail=0;
q[tail++]=start;
seen[start]=1;
while(head<tail){size_t u=q[head++];
out[head-1]=u;
for(size_t v=0;v<g->n;v++)if(g->edges[u*g->n+v]&&!seen[v]){seen[v]=1;
q[tail++]=v;
}}
 free(q);
free(seen);
return tail>INT_MAX?-1:(int)tail;
}
void mulib_graph_free(mulib_graph_t *g){if(g){free(g->edges);
memset(g,0,sizeof(*g));
}}
int mulib_graph_toposort(const mulib_graph_t *g,size_t *order,size_t cap){
 if(!g||!g->edges||(!order&&g->n)||cap<g->n||g->n>SIZE_MAX/sizeof(size_t))return 0;
 size_t *degree=(size_t*)calloc(g->n?g->n:1,sizeof(size_t)),*tmp=(size_t*)malloc((g->n?g->n:1)*sizeof(size_t));
if(!degree||!tmp){free(degree);
free(tmp);
return 0;
}
 for(size_t a=0;a<g->n;a++)for(size_t b=0;b<g->n;b++)degree[b]+=(size_t)!!g->edges[a*g->n+b];
 for(size_t k=0;k<g->n;k++){size_t a=0;
while(a<g->n&&degree[a]!=0)a++;
if(a==g->n){free(degree);
free(tmp);
return 0;
}
 tmp[k]=a;
degree[a]=SIZE_MAX;
for(size_t b=0;b<g->n;b++)if(g->edges[a*g->n+b])degree[b]--;
}
 if(g->n)memcpy(order,tmp,g->n*sizeof(size_t));
free(degree);
free(tmp);
return 1;
}
int mulib_graph_reverse(const mulib_graph_t *g,mulib_graph_t *out){if(!g||!out||out->edges||!g->edges)return 0;
mulib_graph_t tmp={0};
if(!mulib_graph_init(&tmp,g->n))return 0;
for(size_t i=0;i<g->n;i++)for(size_t j=0;j<g->n;j++)tmp.edges[i*g->n+j]=g->edges[j*g->n+i];
*out=tmp;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
