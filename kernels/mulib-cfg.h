/* MuLIB compiler tools: Owned bounded control-flow graph with deterministic reachability and postorder.
 * ISO C11 single-header module. Define MULIB_IMPL_CFG in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_CFG_H
#define MULIB_CFG_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t from,to;
} mulib_cfg_edge_t;
typedef struct {size_t blocks,count,capacity;
mulib_cfg_edge_t *edges;
} mulib_cfg_t;
int mulib_cfg_add(mulib_cfg_t *g,size_t from,size_t to);
int mulib_cfg_has(const mulib_cfg_t *g,size_t from,size_t to);
/* Checks edge storage, endpoints, and counts before analyses read the graph. */
int mulib_cfg_validate(const mulib_cfg_t *g);
int mulib_cfg_reachable(const mulib_cfg_t *g,size_t entry,uint64_t *mask);
/* Postorder visits reachable blocks, child edges in insertion order; count returned. */
int mulib_cfg_postorder(const mulib_cfg_t *g,size_t entry,size_t *order,size_t capacity,size_t *count);
void mulib_cfg_free(mulib_cfg_t *g);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CFG_H */
#if defined(MULIB_IMPL_CFG) && !defined(MULIB_CFG_IMPLEMENTED)
#define MULIB_CFG_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_cfg_validate(const mulib_cfg_t *g){
if(!g||g->count>g->capacity||(!g->edges&&g->count))return 0;
for(size_t i=0;i<g->count;i++)if(g->edges[i].from>=g->blocks||g->edges[i].to>=g->blocks)return 0;
return 1;
}
int mulib_cfg_has(const mulib_cfg_t *g,size_t a,size_t b){if(!g||(!g->edges&&g->count)||a>=g->blocks||b>=g->blocks)return 0;
for(size_t i=0;i<g->count;i++)if(g->edges[i].from==a&&g->edges[i].to==b)return 1;
return 0;
}
int mulib_cfg_add(mulib_cfg_t *g,size_t a,size_t b){if(!g||a>=g->blocks||b>=g->blocks)return 0;
if(mulib_cfg_has(g,a,b))return 1;
 if(g->count==g->capacity){size_t cap=g->capacity?g->capacity*2:16;
if(cap<g->capacity||cap>SIZE_MAX/sizeof(*g->edges))return 0;
void *p=realloc(g->edges,cap*sizeof(*g->edges));
if(!p)return 0;
g->edges=(mulib_cfg_edge_t*)p;
g->capacity=cap;
}g->edges[g->count++]=(mulib_cfg_edge_t){a,b};
return 1;
}
int mulib_cfg_reachable(const mulib_cfg_t *g,size_t entry,uint64_t *out){if(!mulib_cfg_validate(g)||!out||g->blocks>64||!g->blocks||entry>=g->blocks)return 0;
uint64_t reach=UINT64_C(1)<<entry;
int changed;
 do{changed=0;
for(size_t e=0;e<g->count;e++){size_t a=g->edges[e].from,b=g->edges[e].to;
if(a>=g->blocks||b>=g->blocks)return 0;
if((reach&(UINT64_C(1)<<a))&&!(reach&(UINT64_C(1)<<b))){reach|=UINT64_C(1)<<b;
changed=1;
}}}while(changed);
*out=reach;
return 1;
}
int mulib_cfg_postorder(const mulib_cfg_t *g,size_t entry,size_t *order,size_t capacity,size_t *count){if(!mulib_cfg_validate(g)||!order||!count||g->blocks>64||!g->blocks||entry>=g->blocks)return 0;
size_t result[64];
 typedef struct {size_t node,next;
} frame_t;
frame_t stack[64];
size_t depth=1,n=0;
uint64_t seen=UINT64_C(1)<<entry;
stack[0]=(frame_t){entry,0};
 while(depth){frame_t *top=stack+depth-1;
size_t i=top->next;
while(i<g->count){mulib_cfg_edge_t e=g->edges[i];
if(e.from>=g->blocks||e.to>=g->blocks)return 0;
if(e.from==top->node&&!(seen&(UINT64_C(1)<<e.to)))break;
i++;
}
 if(i<g->count){size_t to=g->edges[i].to;
top->next=i+1;
seen|=UINT64_C(1)<<to;
stack[depth++]=(frame_t){to,0};
}
  else {result[n++]=top->node;
depth--;
}}
  if(capacity<n)return 0;
memcpy(order,result,n*sizeof(*order));
*count=n;
return 1;
}
void mulib_cfg_free(mulib_cfg_t *g){if(g){free(g->edges);
memset(g,0,sizeof(*g));
}}
#ifdef __cplusplus
}
#endif
#endif
