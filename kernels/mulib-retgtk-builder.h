/* MuLIB graphical extension: retgtk-builder. C11, header-only. */
#ifndef MULIB_RETGTK_BUILDER_H
#define MULIB_RETGTK_BUILDER_H
#include "mulib-retgtk-widget.h"
typedef mulib_retgtk_widget_t *(*mret_factory)(uint64_t,const char *,void *);
typedef struct{const char *tag;mret_factory factory;void *user;}mret_builder_factory;
typedef struct{const char *tag,*text;uint64_t id;size_t parent;}mret_builder_node;

#ifndef MULIB_RETGTK_BUILDER_MAX_NODES
#define MULIB_RETGTK_BUILDER_MAX_NODES 100000u
#endif
#ifndef MULIB_RETGTK_BUILDER_MAX_DEPTH
#define MULIB_RETGTK_BUILDER_MAX_DEPTH 128u
#endif

static inline int mret_builder_id_compare(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}
static inline int mret_builder_ids_valid(const mret_builder_node *v, size_t n)
{
    if (!v || !n || n > SIZE_MAX / sizeof(uint64_t)) return 0;
    uint64_t *ids = malloc(n * sizeof *ids);
    if (!ids) return 0;
    for (size_t i = 0; i < n; ++i) ids[i] = v[i].id;
    qsort(ids, n, sizeof *ids, mret_builder_id_compare);
    int ok = 1;
    for (size_t i = 1; i < n; ++i)
        if (ids[i - 1] == ids[i]) { ok = 0; break; }
    free(ids);
    return ok;
}

/* Factories return one owned reference to a fresh, unparented widget with the
 * requested ID. They must not mutate an existing tree. All document topology,
 * IDs and factory names are checked before invoking any factory. Failure drops
 * both temporary references and parent-owned references; success returns just
 * the caller-owned root, whose descendants are owned by their parents. */
static inline mulib_retgtk_widget_t *mulib_retgtk_builder_build(
    const mret_builder_factory *f, size_t nf, const mret_builder_node *v, size_t n)
{
    if (!f || !nf || !v || !n || n > MULIB_RETGTK_BUILDER_MAX_NODES ||
        n > SIZE_MAX / sizeof(mulib_retgtk_widget_t *) ||
        v[0].parent != SIZE_MAX) return NULL;
    for (size_t j = 0; j < nf; ++j) {
        if (!f[j].tag || !*f[j].tag || !f[j].factory) return NULL;
        for (size_t k = 0; k < j; ++k)
            if (!strcmp(f[j].tag, f[k].tag)) return NULL;
    }
    for (size_t i = 0; i < n; ++i) {
        if (!v[i].tag || !*v[i].tag || (i && v[i].parent >= i)) return NULL;
        size_t j = 0;
        while (j < nf && strcmp(f[j].tag, v[i].tag)) ++j;
        if (j == nf) return NULL;
        size_t depth = 1;
        for (size_t p = i; p; p = v[p].parent)
            if (++depth > MULIB_RETGTK_BUILDER_MAX_DEPTH) return NULL;
    }
    if (!mret_builder_ids_valid(v, n)) return NULL;
    mulib_retgtk_widget_t **w = calloc(n, sizeof *w);
    if (!w) return NULL;
    for (size_t i = 0; i < n; ++i) {
        size_t j = 0;
        while (strcmp(f[j].tag, v[i].tag)) ++j;
        w[i] = f[j].factory(v[i].id, v[i].text, f[j].user);
        if (!w[i] || w[i]->parent || w[i]->object.id != v[i].id ||
            (i && !mulib_retgtk_widget_add(w[v[i].parent], w[i]))) goto fail;
    }
    mulib_retgtk_widget_t *root = w[0];
    for (size_t i = 1; i < n; ++i) mulib_retgtk_widget_unref(w[i]);
    free(w);
    return root;
fail:
    /* Release in reverse order, leaving parent ownership intact until the root
     * is released. This also handles a factory failing after several subtrees. */
    for (size_t i = n; i; --i) mulib_retgtk_widget_unref(w[i - 1]);
    free(w);
    return NULL;
}
#endif
