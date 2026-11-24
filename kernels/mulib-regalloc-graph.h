/* MuLIB compiler tools: Greedy graph-color register allocation with spill decisions for up to 64 values.
 * ISO C11 single-header module. Define MULIB_IMPL_REGALLOC_GRAPH in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_REGALLOC_GRAPH_H
#define MULIB_REGALLOC_GRAPH_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
/* interference[i] has edge bit j; no self edges; symmetric edges required. */
int mulib_regalloc_graph(const uint64_t *interference,size_t nodes,uint64_t registers,int *colors,size_t *spills);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_REGALLOC_GRAPH_H */
#if defined(MULIB_IMPL_REGALLOC_GRAPH) && !defined(MULIB_REGALLOC_GRAPH_IMPLEMENTED)
#define MULIB_REGALLOC_GRAPH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_regalloc_graph(const uint64_t *graph,size_t n,uint64_t regs,int *out,size_t *spills){
 if((!graph&&n)||(!out&&n)||!spills||n>64)return 0;
int colors[64];
uint64_t valid=n==64?UINT64_MAX:((UINT64_C(1)<<n)-1);
 for(size_t i=0;i<n;i++){if((graph[i]&~valid)||(graph[i]&(UINT64_C(1)<<i)))return 0;
for(size_t j=0;j<n;j++)if(!!(graph[i]&(UINT64_C(1)<<j))!=!!(graph[j]&(UINT64_C(1)<<i)))return 0;
colors[i]=-2;
}
 size_t count=0;
for(size_t step=0;step<n;step++){size_t v=SIZE_MAX,best=0;
for(size_t i=0;i<n;i++)if(colors[i]==-2){size_t deg=0;
for(size_t j=0;j<n;j++)if(graph[i]&(UINT64_C(1)<<j))deg++;
if(v==SIZE_MAX||deg>best){v=i;
best=deg;
}}
 uint64_t used=0;
for(size_t j=0;j<n;j++)if((graph[v]&(UINT64_C(1)<<j))&&colors[j]>=0)used|=UINT64_C(1)<<(unsigned)colors[j];
int chosen=-1;
for(unsigned r=0;r<64;r++)if((regs&(UINT64_C(1)<<r))&&!(used&(UINT64_C(1)<<r))){chosen=(int)r;
break;
}
 colors[v]=chosen;
if(chosen<0)count++;
}
 for(size_t i=0;i<n;i++)out[i]=colors[i];
*spills=count;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
