/** @file mulib-nodegraph-validate.h
 * Validate node identity, port types and unique incoming edges.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Cycle rejection is in nodegraph-compile; unconnected ports allowed.
 */
#ifndef MULIB_NODEGRAPH_VALIDATE_H
#define MULIB_NODEGRAPH_VALIDATE_H
#include "mulib-nodegraph-core.h"
#include "mulib-nodegraph-types.h"
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
static inline bool mulib_nodegraph_validate(mulib_nodegraph g){
    if(!mulib_nodegraph_basic(g))return false;
    for(size_t i=0;i<g.count;i++){
        size_t bytes;
        const mulib_nodegraph_node *n=&g.nodes[i];
        if(!n->id||!n->output_type||
           !mulib_nodegraph_span(n->input_types,n->input_count,sizeof(uint32_t),&bytes))return false;
        for(size_t j=0;j<n->input_count;j++)if(!n->input_types[j])return false;
        for(size_t j=0;j<i;j++)if(n->id==g.nodes[j].id)return false;
    }
    for(size_t i=0;i<g.edge_count;i++){
        mulib_nodegraph_edge e=g.edges[i];
        if(!mulib_nodegraph_types_edge(g,e))return false;
        for(size_t j=0;j<i;j++)if(g.edges[j].to==e.to&&g.edges[j].slot==e.slot)return false;
    }
    return true;
}
/* Validate the whole permutation and edge precedence before any output writes
 * or callbacks. order_count is the accessible size_t capacity. No scratch. */
static inline bool mulib_nodegraph_order_valid(mulib_nodegraph g,const size_t *order,size_t order_count){
    size_t bytes;
    if(!mulib_nodegraph_validate(g)||order_count<g.count||
       !mulib_nodegraph_span(order,g.count,sizeof *order,&bytes))return false;
    for(size_t i=0;i<g.count;i++){
        size_t v=order[i];
        if(v>=g.count)return false;
        for(size_t j=0;j<i;j++)if(order[j]==v)return false;
        for(size_t e=0;e<g.edge_count;e++)if(g.edges[e].to==v){
            size_t j=0;
            while(j<i&&order[j]!=g.edges[e].from)j++;
            if(j==i)return false;
        }
    }
    return true;
}
#ifdef __cplusplus
}
#endif
#endif
