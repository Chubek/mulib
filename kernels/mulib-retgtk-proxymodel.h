/* MuLIB graphical extension: retgtk-proxymodel. C11, header-only. */
#ifndef MULIB_RETGTK_PROXYMODEL_H
#define MULIB_RETGTK_PROXYMODEL_H
#include "mulib-retgtk-model.h"
typedef struct {
    const mulib_retgtk_model_t *source;
    size_t *indices, n;
    size_t source_rows, source_columns;
    uint64_t revision;
} mulib_retgtk_proxymodel_t;

/* Build is transactional. Cell strings are borrowed and must remain stable
 * throughout the call. Equal keys retain source order in BOTH directions.
 * Bottom-up mergesort is O(rows + matches log matches), with O(rows) scratch. */
static inline int mulib_retgtk_proxymodel_build(mulib_retgtk_proxymodel_t *p,
    const mulib_retgtk_model_t *m, size_t column, const char *filter, int ascending)
{
    if (!p || !m || !m->rows || !m->columns || !m->cell) return 0;
    size_t cols = m->columns(m->user), n = m->rows(m->user);
    uint64_t revision = m->revision;
    if (column >= cols || n > SIZE_MAX / sizeof(size_t) ||
        n > SIZE_MAX / sizeof(char *)) return 0;
    size_t count = n ? n : 1;
    size_t *v = malloc(count * sizeof *v);
    size_t *tmp = malloc(count * sizeof *tmp);
    const char **keys = malloc(count * sizeof *keys);
    if (!v || !tmp || !keys) {
        free(v); free(tmp); free(keys);
        return 0;
    }
    size_t k = 0;
    for (size_t i = 0; i < n; ++i) {
        keys[i] = m->cell(m->user, i, column);
        if (keys[i] && (!filter || strstr(keys[i], filter))) v[k++] = i;
    }
    for (size_t width = 1; width < k;) {
        for (size_t begin = 0; begin < k;) {
            size_t mid = begin + (width < k - begin ? width : k - begin);
            size_t end = mid + (width < k - mid ? width : k - mid);
            size_t left = begin, right = mid, out = begin;
            while (left < mid && right < end) {
                int cmp = strcmp(keys[v[left]], keys[v[right]]);
                int take_left = ascending ? cmp <= 0 : cmp >= 0;
                tmp[out++] = take_left ? v[left++] : v[right++];
            }
            while (left < mid) tmp[out++] = v[left++];
            while (right < end) tmp[out++] = v[right++];
            begin = end;
        }
        size_t *swap = v; v = tmp; tmp = swap;
        if (width >= k - width) break;
        width *= 2;
    }
    free(tmp);
    free(keys);
    if (m->revision != revision || m->rows(m->user) != n ||
        m->columns(m->user) != cols) { free(v); return 0; }
    free(p->indices);
    *p = (mulib_retgtk_proxymodel_t){m, v, k, n, cols, revision};
    return 1;
}

/* The source and its interface outlive the proxy. Mutate through model_set or
 * increment that same interface's revision after external edits. Rebuild after
 * edits: stale snapshots return no rows/cells instead of reusing shifted IDs. */
static inline int
mulib_retgtk_proxymodel_current(const mulib_retgtk_proxymodel_t *p)
{
    return p && p->source && p->source->rows && p->source->columns &&
        p->source->revision == p->revision &&
        p->source->rows(p->source->user) == p->source_rows &&
        p->source->columns(p->source->user) == p->source_columns;
}
static inline size_t mulib_retgtk_proxymodel_source_row(
    const mulib_retgtk_proxymodel_t *p, size_t row)
{
    return mulib_retgtk_proxymodel_current(p) && row < p->n && p->indices &&
        p->indices[row] < p->source_rows ? p->indices[row] : SIZE_MAX;
}
static inline const char *mulib_retgtk_proxymodel_cell(
    const mulib_retgtk_proxymodel_t *p, size_t r, size_t c)
{
    size_t row = mulib_retgtk_proxymodel_source_row(p, r);
    return row == SIZE_MAX ? NULL : mulib_retgtk_model_cell(p->source, row, c);
}
static inline size_t mret_proxy_rows(void *u)
{
    mulib_retgtk_proxymodel_t *p = u;
    return mulib_retgtk_proxymodel_current(p) ? p->n : 0;
}
static inline size_t mret_proxy_columns(void *u)
{
    mulib_retgtk_proxymodel_t *p = u;
    return mulib_retgtk_proxymodel_current(p) ? p->source_columns : 0;
}
static inline const char *mret_proxy_cell(void *u, size_t r, size_t c)
{
    return mulib_retgtk_proxymodel_cell(u, r, c);
}
static inline mulib_retgtk_model_t mulib_retgtk_proxymodel_interface(
    mulib_retgtk_proxymodel_t *p)
{
    return (mulib_retgtk_model_t){p, mret_proxy_rows, mret_proxy_columns,
                                 mret_proxy_cell, NULL, 0};
}
static inline void mulib_retgtk_proxymodel_free(mulib_retgtk_proxymodel_t *p)
{
    if (p) { free(p->indices); memset(p, 0, sizeof *p); }
}
#endif
