/* MuLIB: timing-graph. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_GRAPH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_GRAPH_H
#define MULIB_TIMING_GRAPH_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_graph_order(size_t vertices,const mulib_graph_weighted_edge *edges,size_t n,size_t *order);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_GRAPH) && !defined(MULIB_TIMING_GRAPH_H_IMPLEMENTED)
#define MULIB_TIMING_GRAPH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_graph_order(size_t vertices,const mulib_graph_weighted_edge *edges,size_t n,size_t *order){if(!edges&&n)return 0;for(size_t i=0;i<n;i++)if(!isfinite(edges[i].weight)||edges[i].weight<0)return 0;return mulib_graph_edges_toposort(vertices,edges,n,order);}
#ifdef __cplusplus
}
#endif
#endif
