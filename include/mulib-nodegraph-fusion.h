/** @file mulib-nodegraph-fusion.h
 * Conservative candidate detection for unary operator fusion.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Does not rewrite graph; verify execution semantic compatibility.
 */
#ifndef MULIB_NODEGRAPH_FUSION_H
#define MULIB_NODEGRAPH_FUSION_H
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
/* Detect adjacent unary pure nodes with a single consumer of upstream. */
/* pure_count is the accessible bool capacity. This does not prove acyclicity. */
static inline bool mulib_nodegraph_fusion_candidate(mulib_nodegraph g,const bool *pure,size_t pure_count,size_t edge){size_t bytes;if(!mulib_nodegraph_validate(g)||pure_count<g.count||!mulib_nodegraph_span(pure,g.count,sizeof *pure,&bytes)||edge>=g.edge_count)return false;mulib_nodegraph_edge e=g.edges[edge];if(!pure[e.from]||!pure[e.to]||g.nodes[e.to].input_count!=1)return false;size_t count=0;for(size_t i=0;i<g.edge_count;i++)if(g.edges[i].from==e.from)count++;return count==1;}
#ifdef __cplusplus
}
#endif
#endif
