/* MuLIB: hwil-region. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_REGION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_REGION_H
#define MULIB_HWIL_REGION_H
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

int mulib_hwil_region_order(size_t blocks,const mulib_graph_weighted_edge *edges,size_t n,size_t *order);
int mulib_hwil_region_reachable(size_t blocks,const mulib_graph_weighted_edge *edges,size_t n,size_t entry,unsigned char *reachable);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_REGION) && !defined(MULIB_HWIL_REGION_H_IMPLEMENTED)
#define MULIB_HWIL_REGION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_region_order(size_t blocks,const mulib_graph_weighted_edge *edges,size_t n,size_t *order){return mulib_graph_edges_toposort(blocks,edges,n,order);}
int mulib_hwil_region_reachable(size_t blocks,const mulib_graph_weighted_edge *edges,size_t n,size_t entry,unsigned char *reachable){if(entry>=blocks||!reachable||(!edges&&n))return 0;for(size_t i=0;i<n;i++)if(edges[i].from>=blocks||edges[i].to>=blocks)return 0;memset(reachable,0,blocks);reachable[entry]=1;int changed=1;while(changed){changed=0;for(size_t i=0;i<n;i++)if(reachable[edges[i].from]&&!reachable[edges[i].to]){reachable[edges[i].to]=1;changed=1;}}return 1;}
#ifdef __cplusplus
}
#endif
#endif
