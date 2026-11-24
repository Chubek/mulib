/** @file mulib-nodegraph-compile.h
 * Kahn topological ordering and cycle detection.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: O(VE) traversal; add adjacency index.
 */
#ifndef MULIB_NODEGRAPH_COMPILE_H
#define MULIB_NODEGRAPH_COMPILE_H
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
/* Capacities are size_t elements. Disjoint output/scratch and graph storage.
 * A cycle returns false with partial order/degree; other validation is atomic. */
static inline bool mulib_nodegraph_compile(mulib_nodegraph g,size_t *order,size_t order_cap,size_t *degree,size_t degree_cap){
    if(!mulib_nodegraph_validate(g)||order_cap<g.count||degree_cap<g.count||
       !mulib_nodegraph_disjoint(order,g.count,sizeof *order,degree,g.count,sizeof *degree)||
       !mulib_nodegraph_output_disjoint(g,order,g.count,sizeof *order)||
       !mulib_nodegraph_output_disjoint(g,degree,g.count,sizeof *degree))return false;
    for(size_t i=0;i<g.count;i++)degree[i]=0;
    for(size_t i=0;i<g.edge_count;i++)degree[g.edges[i].to]++;
    size_t head=0,tail=0;
    for(size_t i=0;i<g.count;i++)if(!degree[i])order[tail++]=i;
    while(head<tail){
        size_t v=order[head++];
        for(size_t i=0;i<g.edge_count;i++)
            if(g.edges[i].from==v&&!--degree[g.edges[i].to])order[tail++]=g.edges[i].to;
    }
    return tail==g.count;
}
#ifdef __cplusplus
}
#endif
#endif
