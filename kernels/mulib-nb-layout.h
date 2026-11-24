/* SPDX-License-Identifier: MIT */
#ifndef MULIB_NB_LAYOUT_H
#define MULIB_NB_LAYOUT_H
#include "mulib-nb-tree.h"

/* Fenwick index over current ordering. Hidden descendants have zero extent.
 * owner is borrowed: free the index before destroying/moving its document. */
typedef struct {
    double *tree, *extent;
    size_t n;
    double gap;
    uint64_t revision;
    const mulib_nb *owner;
    uint64_t document_id;
} mulib_nb_layout;

static inline void
mulib_nb_layout_free(mulib_nb_layout *l)
{
    if (l) {
        free(l->tree);
        free(l->extent);
        memset(l, 0, sizeof *l);
    }
}

static inline int
mulib_nb_layout_current(const mulib_nb_layout *l, const mulib_nb *n)
{
    return l && n && l->owner == n && l->document_id == n->id
        && l->revision == n->revision && l->n == n->n
        && l->tree && l->extent;
}

/* View-only measurement update. Use set_height + rebuild to edit the document. */
static inline int
mulib_nb_layout_update(mulib_nb_layout *l, size_t i, double extent)
{
    if (!l || !mulib_nb_layout_current(l, l->owner) || i >= l->n
        || !isfinite(extent) || extent < 0)
        return 0;
    double delta = extent - l->extent[i];
    for (size_t k = i + 1; k <= l->n;) {
        if (!isfinite(l->tree[k] + delta))
            return 0;
        size_t step = k & (~k + 1);
        if (step > l->n - k)
            break;
        k += step;
    }
    /* Check the total separately: the root sum may span multiple tree nodes. */
    double total = 0;
    for (size_t k = l->n; k; k -= k & (~k + 1))
        total += l->tree[k];
    if (!isfinite(total + delta))
        return 0;
    for (size_t k = i + 1; k <= l->n;) {
        l->tree[k] += delta;
        size_t step = k & (~k + 1);
        if (step > l->n - k)
            break;
        k += step;
    }
    l->extent[i] = extent;
    return 1;
}

/* Failure leaves the previous index intact. Includes a trailing gap for
 * compatibility. Graph validation and construction are expected O(n). */
static inline int
mulib_nb_layout_build(mulib_nb_layout *l, mulib_nb *n,
                      double gap, double collapsed_height)
{
    if (!l || !n || !n->id || !isfinite(gap) || gap < 0
        || !isfinite(collapsed_height) || collapsed_height <= 0
        || n->n >= SIZE_MAX / sizeof(double))
        return 0;
    unsigned char *visible = NULL;
    if (!mulib_nb_tree_visibility(n, &visible))
        return 0;
    mulib_nb_layout t = {.n = n->n, .gap = gap, .revision = n->revision,
                         .owner = n, .document_id = n->id};
    t.tree = (double *)calloc(n->n + 1, sizeof(double));
    t.extent = (double *)calloc(n->n ? n->n : 1, sizeof(double));
    if (!t.tree || !t.extent)
        goto fail;
    double total = 0;
    for (size_t i = 0; i < n->n; ++i) {
        double height = (n->v[i].flags & MULIB_NB_COLLAPSED)
            ? collapsed_height : n->v[i].height;
        if (!isfinite(height) || height <= 0)
            goto fail;
        double extent = visible[i] ? height + gap : 0;
        total += extent;
        if (!isfinite(extent) || !isfinite(total))
            goto fail;
        t.extent[i] = extent;
        t.tree[i + 1] += extent;
        size_t k = i + 1, step = k & (~k + 1);
        if (step <= n->n - k)
            t.tree[k + step] += t.tree[k];
    }
    free(visible);
    mulib_nb_layout_free(l);
    *l = t;
    return 1;
fail:
    free(visible);
    mulib_nb_layout_free(&t);
    return 0;
}

/* Stale queries return NAN (prefix) / n (at), never stale geometry. */
static inline double
mulib_nb_layout_prefix(const mulib_nb_layout *l, size_t i)
{
    if (!l)
        return 0;
    if (!mulib_nb_layout_current(l, l->owner))
        return NAN;
    if (i > l->n)
        i = l->n;
    double sum = 0;
    while (i) {
        sum += l->tree[i];
        i -= i & (~i + 1);
    }
    return sum;
}

static inline size_t
mulib_nb_layout_at(const mulib_nb_layout *l, double y)
{
    if (!l || !mulib_nb_layout_current(l, l->owner) || !l->n || !isfinite(y))
        return l ? l->n : 0;
    if (y < 0)
        y = 0;
    size_t i = 0, bit = 1;
    while (bit <= l->n / 2)
        bit *= 2;
    for (; bit; bit /= 2) {
        size_t j = i + bit;
        if (j <= l->n && l->tree[j] <= y) {
            i = j;
            y -= l->tree[j];
        }
    }
    return i;
}
#endif
