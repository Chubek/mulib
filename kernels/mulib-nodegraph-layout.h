/** @file mulib-nodegraph-layout.h
 * Layered DAG layout with deterministic rows.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Validates supplied topological order; no crossing minimization.
 */
#ifndef MULIB_NODEGRAPH_LAYOUT_H
#define MULIB_NODEGRAPH_LAYOUT_H
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
typedef struct {size_t layer,row;} mulib_nodegraph_position;
/* Capacities are array elements. Reject invalid orders/aliases before writing. */
static inline bool mulib_nodegraph_layout(mulib_nodegraph g,const size_t *order,size_t order_count,mulib_nodegraph_position *pos,size_t pos_cap,size_t *rows,size_t rows_cap){
    if(!mulib_nodegraph_order_valid(g,order,order_count)||pos_cap<g.count||rows_cap<g.count||
       !mulib_nodegraph_disjoint(pos,g.count,sizeof *pos,rows,g.count,sizeof *rows)||
       !mulib_nodegraph_disjoint(pos,g.count,sizeof *pos,order,g.count,sizeof *order)||
       !mulib_nodegraph_disjoint(rows,g.count,sizeof *rows,order,g.count,sizeof *order)||
       !mulib_nodegraph_output_disjoint(g,pos,g.count,sizeof *pos)||
       !mulib_nodegraph_output_disjoint(g,rows,g.count,sizeof *rows))return false;
    for(size_t i=0;i<g.count;i++){pos[i].layer=0;rows[i]=0;}
    for(size_t i=0;i<g.count;i++){
        size_t v=order[i];
        pos[v].row=rows[pos[v].layer]++;
        for(size_t j=0;j<g.edge_count;j++)if(g.edges[j].from==v){
            size_t w=g.edges[j].to;
            if(pos[w].layer<=pos[v].layer)pos[w].layer=pos[v].layer+1;
        }
    }
    return true;
}
#ifdef __cplusplus
}
#endif
#endif
