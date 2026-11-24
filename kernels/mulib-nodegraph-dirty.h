/** @file mulib-nodegraph-dirty.h
 * Propagate invalidation downstream to fixed point.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Quadratic worst case; use adjacency and queue.
 */
#ifndef MULIB_NODEGRAPH_DIRTY_H
#define MULIB_NODEGRAPH_DIRTY_H
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
/* dirty_cap is bool elements; dirty must not overlap graph storage. */
static inline bool mulib_nodegraph_dirty_propagate(mulib_nodegraph g,bool *dirty,size_t dirty_cap){if(!mulib_nodegraph_validate(g)||dirty_cap<g.count||!mulib_nodegraph_output_disjoint(g,dirty,g.count,sizeof *dirty))return false;for(size_t p=0;p<g.count;p++){bool change=false;for(size_t i=0;i<g.edge_count;i++){mulib_nodegraph_edge e=g.edges[i];if(dirty[e.from]&&!dirty[e.to]){dirty[e.to]=true;change=true;}}if(!change)break;}return true;}
#ifdef __cplusplus
}
#endif
#endif
