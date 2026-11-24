/** @file mulib-nodegraph-eval.h
 * Execute a topologically ordered graph through an operation provider.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Callback manages values and cleanup; no parallel execution.
 */
#ifndef MULIB_NODEGRAPH_EVAL_H
#define MULIB_NODEGRAPH_EVAL_H
#include "mulib-nodegraph-core.h"
#include "mulib-nodegraph-validate.h"
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
typedef bool (*mulib_nodegraph_eval_fn)(void*,size_t,const size_t*,size_t,uintptr_t*);
/* Capacities are elements of the corresponding array. Scratch entries are
 * upstream indices or SIZE_MAX. Arrays may not overlap topology or each other.
 * Trusted callbacks may not mutate graph/order/array storage except *out, nor
 * retain scratch/out borrows. Callback failure leaves prior values populated;
 * caller owns cleanup. Invalid orders/capacities invoke no callbacks. */
static inline bool mulib_nodegraph_eval(mulib_nodegraph g,const size_t *order,size_t order_count,uintptr_t *values,size_t value_cap,size_t *scratch,size_t cap,mulib_nodegraph_eval_fn fn,void *ctx){
    size_t need=0;
    if(!fn||!mulib_nodegraph_order_valid(g,order,order_count)||value_cap<g.count)return false;
    for(size_t i=0;i<g.count;i++)if(g.nodes[i].input_count>need)need=g.nodes[i].input_count;
    if(cap<need||
       !mulib_nodegraph_disjoint(values,g.count,sizeof *values,order,g.count,sizeof *order)||
       !mulib_nodegraph_disjoint(scratch,need,sizeof *scratch,order,g.count,sizeof *order)||
       !mulib_nodegraph_disjoint(scratch,need,sizeof *scratch,values,g.count,sizeof *values)||
       !mulib_nodegraph_output_disjoint(g,values,g.count,sizeof *values)||
       !mulib_nodegraph_output_disjoint(g,scratch,need,sizeof *scratch))return false;
    for(size_t k=0;k<g.count;k++){
        size_t v=order[k],n=g.nodes[v].input_count;
        for(size_t i=0;i<n;i++)scratch[i]=SIZE_MAX;
        for(size_t i=0;i<g.edge_count;i++)if(g.edges[i].to==v)
            scratch[g.edges[i].slot]=g.edges[i].from;
        if(!fn(ctx,v,scratch,n,&values[v]))return false;
    }
    return true;
}
#ifdef __cplusplus
}
#endif
#endif
