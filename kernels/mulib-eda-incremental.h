/** @file mulib-eda-incremental.h
 * @brief Dense adjacency invalidation propagation including cyclic dependencies; intended for small
 * incremental graphs (O(n²) memory), not the final million-node build graph.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_INCREMENTAL in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_INCREMENTAL_H
#define MULIB_EDA_INCREMENTAL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_incremental {
    uint8_t *edge, *dirty;
    size_t count;
} mulib_eda_incremental_t;
int mulib_eda_incremental_init(mulib_eda_incremental_t *g, size_t nodes);
int mulib_eda_incremental_depend(mulib_eda_incremental_t *g, size_t prerequisite, size_t dependent);
int mulib_eda_incremental_invalidate(mulib_eda_incremental_t *g, size_t changed);
void mulib_eda_incremental_clear(mulib_eda_incremental_t *g);
void mulib_eda_incremental_free(mulib_eda_incremental_t *g);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_INCREMENTAL_H */
#if defined(MULIB_IMPL_EDA_INCREMENTAL) && !defined(MULIB_EDA_INCREMENTAL_H_IMPLEMENTED)
#define MULIB_EDA_INCREMENTAL_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_incremental_init(mulib_eda_incremental_t *g, size_t n) {
    if (!g || (n && n > SIZE_MAX / n))
        return 0;
    memset(g, 0, sizeof *g);
    if (!n)
        return 1;
    g->edge = calloc(n, n);
    g->dirty = calloc(n, 1);
    if (!g->edge || !g->dirty) {
        mulib_eda_incremental_free(g);
        return 0;
    }
    g->count = n;
    return 1;
}
void mulib_eda_incremental_free(mulib_eda_incremental_t *g) {
    if (!g)
        return;
    free(g->edge);
    free(g->dirty);
    memset(g, 0, sizeof *g);
}
int mulib_eda_incremental_depend(mulib_eda_incremental_t *g, size_t a, size_t b) {
    if (!g || a >= g->count || b >= g->count || a == b)
        return 0;
    g->edge[a * g->count + b] = 1;
    return 1;
}
int mulib_eda_incremental_invalidate(mulib_eda_incremental_t *g, size_t a) {
    if (!g || a >= g->count)
        return 0;
    g->dirty[a] = 1;
    for (size_t k = 0; k < g->count; k++) {
        int changed = 0;
        for (size_t i = 0; i < g->count; i++)
            if (g->dirty[i])
                for (size_t j = 0; j < g->count; j++)
                    if (g->edge[i * g->count + j] && !g->dirty[j]) {
                        g->dirty[j] = 1;
                        changed = 1;
                    }
        if (!changed)
            break;
    }
    return 1;
}
void mulib_eda_incremental_clear(mulib_eda_incremental_t *g) {
    if (g && g->count)
        memset(g->dirty, 0, g->count);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_INCREMENTAL */
