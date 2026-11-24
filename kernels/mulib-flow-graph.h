/** @file mulib-flow-graph.h
 * @brief Deterministic topological sort and cycle detection on up to 64 jobs with dense adjacency.
 * @details C11 single-header implementation; define MULIB_IMPL_FLOW_GRAPH in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FLOW_GRAPH_H
#define MULIB_FLOW_GRAPH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_flow_graph {
    size_t count;
    uint8_t edges[64][64];
} mulib_flow_graph_t;
int mulib_flow_graph_add(mulib_flow_graph_t *g, size_t count);
int mulib_flow_graph_depend(mulib_flow_graph_t *g, uint8_t prerequisite, uint8_t dependent);
int mulib_flow_graph_toposort(const mulib_flow_graph_t *g, uint8_t order[64]);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FLOW_GRAPH_H */
#if defined(MULIB_IMPL_FLOW_GRAPH) && !defined(MULIB_FLOW_GRAPH_H_IMPLEMENTED)
#define MULIB_FLOW_GRAPH_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_flow_graph_add(mulib_flow_graph_t *g, size_t count) {
    if (!g || count > 64)
        return 0;
    memset(g, 0, sizeof *g);
    g->count = count;
    return 1;
}
int mulib_flow_graph_depend(mulib_flow_graph_t *g, uint8_t a, uint8_t b) {
    if (!g || g->count > 64 || a >= g->count || b >= g->count || a == b)
        return 0;
    g->edges[a][b] = 1;
    return 1;
}
int mulib_flow_graph_toposort(const mulib_flow_graph_t *g, uint8_t order[64]) {
    if (!g || g->count > 64 || !order)
        return 0;
    uint8_t chosen[64] = {0};
    for (size_t step = 0; step < g->count; step++) {
        size_t pick = g->count;
        for (size_t i = 0; i < g->count; i++) {
            if (chosen[i])
                continue;
            int ready = 1;
            for (size_t k = 0; k < g->count; k++)
                if (g->edges[k][i] && !chosen[k])
                    ready = 0;
            if (ready) {
                pick = i;
                break;
            }
        }
        if (pick == g->count)
            return 0;
        chosen[pick] = 1;
        order[step] = (uint8_t)pick;
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FLOW_GRAPH */
