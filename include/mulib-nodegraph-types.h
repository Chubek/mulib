/** @file mulib-nodegraph-types.h
 * Exact port-type equality and edge validation.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NODEGRAPH_TYPES_H
#define MULIB_NODEGRAPH_TYPES_H
#include "mulib-nodegraph-core.h"
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
static inline bool mulib_nodegraph_types_match(uint32_t a,uint32_t b){return a&&b&&a==b;}
static inline bool mulib_nodegraph_types_edge(mulib_nodegraph g,mulib_nodegraph_edge e){if(!mulib_nodegraph_basic(g)||e.from>=g.count||e.to>=g.count||e.from==e.to)return false;size_t bytes;const mulib_nodegraph_node *a=&g.nodes[e.from],*b=&g.nodes[e.to];return mulib_nodegraph_span(b->input_types,b->input_count,sizeof(uint32_t),&bytes)&&e.slot<b->input_count&&mulib_nodegraph_types_match(a->output_type,b->input_types[e.slot]);}
#ifdef __cplusplus
}
#endif
#endif
