/* SPDX-License-Identifier: MIT */
#ifndef MULIB_NB_CORE_H
#define MULIB_NB_CORE_H
#include "mulib-nb-cell.h"

typedef struct {
    uint64_t id, next, revision;
    char *name;
    mulib_nb_cell *v;
    size_t n, cap;
    /* Zero policy limits select finite defaults. bytes includes text NULs. */
    size_t cell_limit, text_limit, bytes;
} mulib_nb;

static inline int
mulib_nb_empty(const mulib_nb *n)
{
    return n && !n->id && !n->name && !n->v && !n->n && !n->cap;
}

static inline int
mulib_nb_init(mulib_nb *n, uint64_t id, const char *name)
{
    size_t length;
    if (!mulib_nb_empty(n) || !id
        || !mulib_nb_text_length(name, MULIB_NB_LABEL_MAX, &length))
        return 0;
    char *s = mulib_nb_text_copy(name, length);
    if (!s)
        return 0;
    size_t cells = n->cell_limit, bytes = n->text_limit;
    if (bytes && length + 1 > bytes) {
        free(s);
        return 0;
    }
    *n = (mulib_nb){.id = id, .name = s, .cell_limit = cells,
                    .text_limit = bytes, .bytes = length + 1};
    return 1;
}

static inline mulib_nb_cell *
mulib_nb_find(mulib_nb *n, uint64_t id)
{
    if (n && id)
        for (size_t i = 0; i < n->n; ++i)
            if (n->v[i].id == id)
                return n->v + i;
    return NULL;
}

static inline int
mulib_nb_has_bytes(const mulib_nb *n, size_t add)
{
    size_t limit = n->text_limit ? n->text_limit : MULIB_NB_DEFAULT_BYTES;
    return n->bytes <= limit && add <= limit - n->bytes;
}

static inline uint64_t
mulib_nb_insert(mulib_nb *n, size_t at, mulib_nb_kind kind,
                const char *src, const char *lang, double height)
{
    size_t ns, nl;
    if (!n || !n->id || !n->name || at > n->n
        || n->n >= (n->cell_limit ? n->cell_limit : MULIB_NB_DEFAULT_CELLS)
        || n->next == UINT64_MAX || n->revision == UINT64_MAX
        || kind < MULIB_NB_MARKDOWN || kind > MULIB_NB_RAW
        || !isfinite(height) || height < 0.001 || height > 1e9
        || !mulib_nb_text_length(src ? src : "", MULIB_NB_TEXT_MAX, &ns)
        || !mulib_nb_text_length(lang ? lang : "", MULIB_NB_LABEL_MAX, &nl)
        || !mulib_nb_has_bytes(n, ns + nl + 2))
        return 0;
    mulib_nb_cell c = {.id = n->next + 1, .kind = kind, .height = height};
    if (!mulib_nb_cell_text_n(&c, src ? src : "", ns, lang ? lang : "", nl)
        || !mulib_wm_grow((void **)&n->v, &n->cap, n->n + 1, sizeof c)) {
        mulib_nb_cell_free(&c);
        return 0;
    }
    memmove(n->v + at + 1, n->v + at, (n->n - at) * sizeof c);
    n->v[at] = c;
    ++n->n;
    ++n->next;
    ++n->revision;
    n->bytes += ns + nl + 2;
    return c.id;
}

static inline int
mulib_nb_edit(mulib_nb *n, uint64_t id, const char *src, const char *lang)
{
    mulib_nb_cell *c = mulib_nb_find(n, id);
    size_t ns, nl;
    if (!c || n->revision == UINT64_MAX
        || !mulib_nb_text_length(src, MULIB_NB_TEXT_MAX, &ns)
        || !mulib_nb_text_length(lang, MULIB_NB_LABEL_MAX, &nl))
        return 0;
    size_t old = strlen(c->source) + strlen(c->language) + 2;
    size_t limit = n->text_limit ? n->text_limit : MULIB_NB_DEFAULT_BYTES;
    if (n->bytes < old || n->bytes - old > limit
        || ns + nl + 2 > limit - (n->bytes - old)
        || !mulib_nb_cell_text_n(c, src, ns, lang, nl))
        return 0;
    n->bytes = n->bytes - old + ns + nl + 2;
    ++n->revision;
    return 1;
}

static inline int
mulib_nb_set_height(mulib_nb *n, uint64_t id, double height)
{
    mulib_nb_cell *c = mulib_nb_find(n, id);
    if (!c || !isfinite(height) || height < 0.001 || height > 1e9)
        return 0;
    if (height == c->height)
        return 1;
    if (n->revision == UINT64_MAX)
        return 0;
    c->height = height;
    ++n->revision;
    return 1;
}

static inline int
mulib_nb_remove(mulib_nb *n, uint64_t id)
{
    mulib_nb_cell *c = mulib_nb_find(n, id);
    if (!c || c->window || n->revision == UINT64_MAX)
        return 0;
    size_t k = (size_t)(c - n->v);
    uint64_t parent = c->parent;
    n->bytes -= strlen(c->source) + strlen(c->language) + 2;
    mulib_nb_cell_free(c);
    memmove(c, c + 1, (n->n - k - 1) * sizeof *c);
    --n->n;
    for (size_t i = 0; i < n->n; ++i)
        if (n->v[i].parent == id)
            n->v[i].parent = parent;
    ++n->revision;
    return 1;
}

static inline void
mulib_nb_free(mulib_nb *n)
{
    if (n) {
        for (size_t i = 0; i < n->n; ++i)
            mulib_nb_cell_free(n->v + i);
        free(n->v);
        free(n->name);
        memset(n, 0, sizeof *n);
    }
}

/* Deep document clone; native associations and execution trust never duplicate. */
static inline int
mulib_nb_clone(const mulib_nb *n, mulib_nb *out)
{
    if (!n || !mulib_nb_empty(out) || n == out)
        return 0;
    mulib_nb t = {.cell_limit = n->cell_limit, .text_limit = n->text_limit};
    if (!mulib_nb_init(&t, n->id, n->name))
        return 0;
    for (size_t i = 0; i < n->n; ++i) {
        const mulib_nb_cell *c = n->v + i;
        if (!mulib_nb_insert(&t, t.n, c->kind, c->source, c->language, c->height)) {
            mulib_nb_free(&t);
            return 0;
        }
        mulib_nb_cell *d = t.v + i;
        d->id = c->id;
        d->parent = c->parent;
        d->flags = c->flags & MULIB_NB_COLLAPSED;
        d->revision = c->revision;
    }
    t.next = n->next;
    t.revision = n->revision;
    *out = t;
    return 1;
}
#endif
