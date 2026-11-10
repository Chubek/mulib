/* MuLIB compiler tools: Selector for linear-scan and interference-graph register allocators.
 * ISO C11 single-header module. Define MULIB_IMPL_REGALLOC in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_REGALLOC_H
#define MULIB_REGALLOC_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-regalloc-linear.h"
#include "mulib-regalloc-graph.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-regalloc-linear.h"
#include "mulib-regalloc-graph.h"
typedef enum {MULIB_REGALLOC_LINEAR,MULIB_REGALLOC_GRAPH} mulib_regalloc_algorithm_t;
int mulib_regalloc_allocate(mulib_regalloc_algorithm_t algo,const mulib_liveinterval_t *ranges,size_t count,const uint64_t *interference,uint64_t registers,mulib_regalloc_location_t *locations,size_t *spills);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_REGALLOC_H */
#if defined(MULIB_IMPL_REGALLOC) && !defined(MULIB_REGALLOC_IMPLEMENTED)
#define MULIB_REGALLOC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_regalloc_allocate(mulib_regalloc_algorithm_t algo,const mulib_liveinterval_t *ranges,size_t count,const uint64_t *graph,uint64_t regs,mulib_regalloc_location_t *locations,size_t *spills){
 if(algo==MULIB_REGALLOC_LINEAR)return mulib_regalloc_linear(ranges,count,regs,locations,spills);
 if(algo!=MULIB_REGALLOC_GRAPH||(!locations&&count)||!spills||count>64)return 0;
 int colors[64];
size_t n=0;
if(!mulib_regalloc_graph(graph,count,regs,colors,&n))return 0;
 size_t slot=0;
for(size_t i=0;i<count;i++){locations[i].reg=colors[i];
locations[i].spill=colors[i]>=0?SIZE_MAX:slot++;
}*spills=n;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
