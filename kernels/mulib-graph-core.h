/** @file mulib-graph-core.h
 * @brief Weighted edge lists, disjoint sets and bounded grid routing.
 * C11 header-only substrate; standard library and libm only.
 * Pointer/count arguments require live arrays of the declared lengths.
 * Stateful objects require caller synchronization.
 */
#ifndef MULIB_GRAPH_CORE_H
#define MULIB_GRAPH_CORE_H
#include "mulib-storage-core.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Vertices and grid cells use zero-based size_t indices. Edge weights are
 * doubles; Dijkstra requires finite nonnegative weights. Set operations
 * require valid parent arrays/indices. Toposort may partially write on a
 * cycle. Grid capacity and returned length count size_t path elements;
 * blocked cells are bytes. All temporary allocations use the C allocator. */
typedef struct {size_t from,to; double weight;} mulib_graph_weighted_edge;
static inline size_t mulib_graph_set_find(size_t *p,size_t a){while(p[a]!=a){p[a]=p[p[a]];a=p[a];}return a;}
static inline void mulib_graph_set_union(size_t *p,size_t a,size_t b){a=mulib_graph_set_find(p,a);b=mulib_graph_set_find(p,b);if(a!=b){if(a<b)p[b]=a;else p[a]=b;}}
static inline int mulib_graph_edges_toposort(size_t n,const mulib_graph_weighted_edge *e,size_t m,size_t *out){if((!e&&m)||(!out&&n))return 0;size_t *d=(size_t*)mulib_storage_alloc(n,sizeof(size_t));if(!d)return 0;for(size_t i=0;i<m;i++){if(e[i].from>=n||e[i].to>=n||d[e[i].to]==SIZE_MAX){free(d);return 0;}d[e[i].to]++;}size_t head=0,tail=0;for(size_t i=0;i<n;i++)if(!d[i])out[tail++]=i;while(head<tail){size_t u=out[head++];for(size_t j=0;j<m;j++)if(e[j].from==u&&!--d[e[j].to])out[tail++]=e[j].to;}free(d);return tail==n;}
/* Dijkstra over nonnegative edges, directed; returns 1 even for unreachable vertices. */
static inline int mulib_graph_dijkstra(size_t n,const mulib_graph_weighted_edge *e,size_t m,size_t src,double *dist,size_t *prev){if(src>=n||!dist||!prev||(!e&&m))return 0;for(size_t j=0;j<m;j++)if(e[j].from>=n||e[j].to>=n||e[j].weight<0||!isfinite(e[j].weight))return 0;unsigned char *done=(unsigned char*)mulib_storage_alloc(n,1);if(!done)return 0;for(size_t i=0;i<n;i++){dist[i]=INFINITY;prev[i]=MULIB_STORAGE_NONE;}dist[src]=0;for(size_t k=0;k<n;k++){size_t u=MULIB_STORAGE_NONE;for(size_t i=0;i<n;i++)if(!done[i]&&(u==MULIB_STORAGE_NONE||dist[i]<dist[u]))u=i;if(u==MULIB_STORAGE_NONE||!isfinite(dist[u]))break;done[u]=1;for(size_t j=0;j<m;j++)if(e[j].from==u){size_t v=e[j].to;double d=dist[u]+e[j].weight;if(d<dist[v]){dist[v]=d;prev[v]=u;}}}free(done);return 1;}
/* Four-neighbor A* with Manhattan heuristic, unit edge weights. blocked cells are 1. */
static inline int mulib_graph_grid_route(size_t w,size_t h,const unsigned char *blocked,size_t start,size_t goal,size_t *path,size_t cap,size_t *len){size_t n;if(!w||!h||!mulib_storage_size_mul(w,h,&n)||!blocked||!len||start>=n||goal>=n||blocked[start]||blocked[goal])return 0;size_t *g=(size_t*)mulib_storage_alloc(n,sizeof(size_t)),*p=(size_t*)mulib_storage_alloc(n,sizeof(size_t));unsigned char *closed=(unsigned char*)mulib_storage_alloc(n,1);if(!g||!p||!closed){free(g);free(p);free(closed);return 0;}for(size_t i=0;i<n;i++){g[i]=MULIB_STORAGE_NONE;p[i]=MULIB_STORAGE_NONE;}g[start]=0;int ok=0;for(size_t iter=0;iter<n;iter++){size_t u=MULIB_STORAGE_NONE;double best=INFINITY;for(size_t i=0;i<n;i++)if(!closed[i]&&g[i]!=MULIB_STORAGE_NONE){double f=(double)g[i]+fabs((double)(i%w)-(double)(goal%w))+fabs((double)(i/w)-(double)(goal/w));if(f<best){best=f;u=i;}}if(u==MULIB_STORAGE_NONE)break;if(u==goal){ok=1;break;}closed[u]=1;size_t nb[4]={u%w?u-1:MULIB_STORAGE_NONE,u%w+1<w?u+1:MULIB_STORAGE_NONE,u>=w?u-w:MULIB_STORAGE_NONE,u/w+1<h?u+w:MULIB_STORAGE_NONE};for(size_t j=0;j<4;j++){size_t v=nb[j];if(v!=MULIB_STORAGE_NONE&&!blocked[v]&&!closed[v]&&g[u]+1<g[v]){g[v]=g[u]+1;p[v]=u;}}}if(ok){*len=g[goal]+1;if(!path||cap<*len)ok=0;else{size_t u=goal;for(size_t i=*len;i-->0;){path[i]=u;u=p[u];}}}else *len=0;free(g);free(p);free(closed);return ok;}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GRAPH_CORE_H */
