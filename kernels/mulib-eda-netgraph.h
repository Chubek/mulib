/** @file mulib-eda-netgraph.h
 * @brief Append-only connectivity hypergraph with indexed nets, terminals, explicit disconnection,
 * net merging and deterministic degrees.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_NETGRAPH in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_NETGRAPH_H
#define MULIB_EDA_NETGRAPH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_terminal {
    uint32_t net;
    uint16_t kind;
} mulib_eda_terminal_t;
typedef struct mulib_eda_netgraph {
    mulib_eda_terminal_t *term;
    size_t count, capacity;
    uint32_t nets;
} mulib_eda_netgraph_t;
int mulib_eda_netgraph_net(mulib_eda_netgraph_t *g, uint32_t *id);
int mulib_eda_netgraph_terminal(mulib_eda_netgraph_t *g, uint16_t kind, uint32_t *id);
int mulib_eda_netgraph_connect(mulib_eda_netgraph_t *g, uint32_t terminal, uint32_t net);
size_t mulib_eda_netgraph_degree(const mulib_eda_netgraph_t *g, uint32_t net);
void mulib_eda_netgraph_free(mulib_eda_netgraph_t *g);
int mulib_eda_netgraph_merge(mulib_eda_netgraph_t *g, uint32_t target, uint32_t source);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_NETGRAPH_H */
#if defined(MULIB_IMPL_EDA_NETGRAPH) && !defined(MULIB_EDA_NETGRAPH_H_IMPLEMENTED)
#define MULIB_EDA_NETGRAPH_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_eda_netgraph_free(mulib_eda_netgraph_t *g) {
    if (!g)
        return;
    free(g->term);
    memset(g, 0, sizeof *g);
}
int mulib_eda_netgraph_net(mulib_eda_netgraph_t *g, uint32_t *id) {
    if (!g || g->nets == UINT32_MAX)
        return 0;
    if (id)
        *id = g->nets;
    g->nets++;
    return 1;
}
int mulib_eda_netgraph_terminal(mulib_eda_netgraph_t *g, uint16_t kind, uint32_t *id) {
    if (!g || !kind || g->count >= UINT32_MAX)
        return 0;
    if (g->count == g->capacity) {
        size_t n = g->capacity ? g->capacity * 2 : 8;
        if (n <= g->capacity || n > SIZE_MAX / sizeof *g->term)
            return 0;
        void *p = realloc(g->term, n * sizeof *g->term);
        if (!p)
            return 0;
        g->term = p;
        g->capacity = n;
    }
    uint32_t k = (uint32_t)g->count++;
    g->term[k].kind = kind;
    g->term[k].net = UINT32_MAX;
    if (id)
        *id = k;
    return 1;
}
int mulib_eda_netgraph_connect(mulib_eda_netgraph_t *g, uint32_t t, uint32_t n) {
    if (!g || t >= g->count || (n != UINT32_MAX && n >= g->nets))
        return 0;
    g->term[t].net = n;
    return 1;
}
size_t mulib_eda_netgraph_degree(const mulib_eda_netgraph_t *g, uint32_t n) {
    size_t z = 0;
    if (!g || n >= g->nets)
        return 0;
    for (size_t i = 0; i < g->count; i++)
        if (g->term[i].net == n)
            z++;
    return z;
}
int mulib_eda_netgraph_merge(mulib_eda_netgraph_t *g, uint32_t target, uint32_t source) {
    if (!g || target >= g->nets || source >= g->nets)
        return 0;
    for (size_t i = 0; i < g->count; i++)
        if (g->term[i].net == source)
            g->term[i].net = target;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_NETGRAPH */
