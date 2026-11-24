/** @file mulib-nodegraph-region.h
 * Backward ROI union with per-node filter halo.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Validates supplied topological order; transform-specific ROI mapping remains.
 */
#ifndef MULIB_NODEGRAPH_REGION_H
#define MULIB_NODEGRAPH_REGION_H
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
typedef struct {int32_t x0,y0,x1,y1;} mulib_nodegraph_region;
static inline bool mulib_nodegraph_region_empty(mulib_nodegraph_region a){return a.x0>=a.x1||a.y0>=a.y1;}
static inline mulib_nodegraph_region mulib_nodegraph_region_union(mulib_nodegraph_region a,mulib_nodegraph_region b){if(mulib_nodegraph_region_empty(a))return b;if(mulib_nodegraph_region_empty(b))return a;mulib_nodegraph_region r={a.x0<b.x0?a.x0:b.x0,a.y0<b.y0?a.y0:b.y0,a.x1>b.x1?a.x1:b.x1,a.y1>b.y1?a.y1:b.y1};return r;}
static inline int32_t mulib_nodegraph_region_sat(int64_t x){return x<INT32_MIN?INT32_MIN:x>INT32_MAX?INT32_MAX:(int32_t)x;}
static inline mulib_nodegraph_region mulib_nodegraph_region_expand(mulib_nodegraph_region a,uint32_t r){if(!mulib_nodegraph_region_empty(a)){a.x0=mulib_nodegraph_region_sat((int64_t)a.x0-r);a.y0=mulib_nodegraph_region_sat((int64_t)a.y0-r);a.x1=mulib_nodegraph_region_sat((int64_t)a.x1+r);a.y1=mulib_nodegraph_region_sat((int64_t)a.y1+r);}return a;}
/* Capacities are elements; halo is in pixels. Invalid order/capacity/alias
 * failures leave regions untouched. Expansion saturates at int32 bounds. */
static inline bool mulib_nodegraph_region_backprop(mulib_nodegraph g,const size_t *order,size_t order_count,mulib_nodegraph_region *r,size_t region_cap,const uint32_t *halo,size_t halo_count){
    if(!mulib_nodegraph_order_valid(g,order,order_count)||region_cap<g.count||halo_count<g.count||
       !mulib_nodegraph_disjoint(r,g.count,sizeof *r,order,g.count,sizeof *order)||
       !mulib_nodegraph_disjoint(r,g.count,sizeof *r,halo,g.count,sizeof *halo)||
       !mulib_nodegraph_output_disjoint(g,r,g.count,sizeof *r))return false;
    for(size_t k=g.count;k;k--){
        size_t v=order[k-1];
        for(size_t j=0;j<g.edge_count;j++)if(g.edges[j].to==v){
            size_t f=g.edges[j].from;
            r[f]=mulib_nodegraph_region_union(r[f],mulib_nodegraph_region_expand(r[v],halo[v]));
        }
    }
    return true;
}
#ifdef __cplusplus
}
#endif
#endif
