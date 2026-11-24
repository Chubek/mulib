/** @file mulib-nodegraph-core.h
 * Borrowed graph topology with indexed edges and typed single outputs.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NODEGRAPH_CORE_H
#define MULIB_NODEGRAPH_CORE_H
#include "mulib-media-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t id;uint32_t type;uint32_t output_type;uint32_t input_count;const uint32_t *input_types;} mulib_nodegraph_node;
typedef struct {size_t from,to;uint32_t slot;} mulib_nodegraph_edge;
typedef struct {const mulib_nodegraph_node *nodes;size_t count;const mulib_nodegraph_edge *edges;size_t edge_count;} mulib_nodegraph;
/* Counts describe actual accessible arrays, not untrusted file metadata. All
 * graph storage is borrowed and immutable for the entire synchronous call.
 * Callers must impose graph-size/work budgets (validation is quadratic). */
static inline bool mulib_nodegraph_span(const void *p,size_t n,size_t element,size_t *bytes){
    return bytes&&(!n||p)&&mulib_media_size_mul(n,element,bytes)&&
        *bytes<=UINTPTR_MAX-(uintptr_t)p;
}
static inline bool mulib_nodegraph_disjoint(const void *a,size_t an,size_t ae,const void *b,size_t bn,size_t be){
    size_t ab,bb;
    return mulib_nodegraph_span(a,an,ae,&ab)&&mulib_nodegraph_span(b,bn,be,&bb)&&
        (!ab||!bb||(uintptr_t)a+ab<=(uintptr_t)b||(uintptr_t)b+bb<=(uintptr_t)a);
}
static inline bool mulib_nodegraph_basic(mulib_nodegraph g){
    size_t n;
    return mulib_nodegraph_span(g.nodes,g.count,sizeof *g.nodes,&n)&&
        mulib_nodegraph_span(g.edges,g.edge_count,sizeof *g.edges,&n);
}
/* Call after validation; rejects writes into topology or input-type arrays.
 * p is output-only storage: this helper inspects its address, never its contents,
 * and accepts uninitialized arrays. Keep its pointer mutable to express that
 * contract to compilers' uninitialized-read analysis. */
static inline bool mulib_nodegraph_output_disjoint(mulib_nodegraph g,void *p,size_t n,size_t element){
    if(!mulib_nodegraph_disjoint(p,n,element,g.nodes,g.count,sizeof *g.nodes)||
       !mulib_nodegraph_disjoint(p,n,element,g.edges,g.edge_count,sizeof *g.edges))return false;
    for(size_t i=0;i<g.count;i++)
        if(!mulib_nodegraph_disjoint(p,n,element,g.nodes[i].input_types,
                                    g.nodes[i].input_count,sizeof(uint32_t)))return false;
    return true;
}
static inline size_t mulib_nodegraph_find(mulib_nodegraph g,uint64_t id){if(mulib_nodegraph_basic(g))for(size_t i=0;i<g.count;i++)if(g.nodes[i].id==id)return i;return SIZE_MAX;}
#ifdef __cplusplus
}
#endif
#endif
