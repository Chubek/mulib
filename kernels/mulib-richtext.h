/* MuLIB graphical extension: richtext. C11, header-only. */
#ifndef MULIB_RICHTEXT_H
#define MULIB_RICHTEXT_H
#include "mulib-textbuffer.h"
typedef struct { size_t start, end; uint32_t color, flags; } mgx_richspan;
typedef mgx_richspan mulib_richspan_t;
typedef struct {
    mulib_textbuffer_t text;
    mgx_richspan *v;
    size_t n, cap;
} mulib_richtext_t;

static inline int mgx_richtext_storage(const mulib_richtext_t *r)
{
    return r && r->n <= r->cap && r->cap <= SIZE_MAX / sizeof *r->v &&
        (r->v ? r->cap != 0 : r->cap == 0);
}

/* Set text and invalidate all old spans only after a successful replacement. */
static inline int mulib_richtext_set(mulib_richtext_t *r,
                                    const char *s, size_t n)
{
    if (!mgx_richtext_storage(r) || !mulib_textbuffer_set(&r->text, s, n))
        return 0;
    r->n = 0;
    return 1;
}

static inline int mulib_richtext_span(mulib_richtext_t *r, mgx_richspan s)
{
    if (!mgx_richtext_storage(r) || r->n == SIZE_MAX / sizeof *r->v ||
        !mulib_textbuffer_valid(&r->text) || s.start >= s.end ||
        !mgx_utf8_boundary(r->text.data, r->text.n, s.start) ||
        !mgx_utf8_boundary(r->text.data, r->text.n, s.end) ||
        !mgx_grow((void **)&r->v, &r->cap, r->n + 1, sizeof s)) return 0;
    r->v[r->n++] = s;
    return 1;
}

/* Last span wins. Invalid/out-of-range queries return the default style.
 * Direct text mutation is discouraged; stale non-boundary spans are ignored. */
static inline mgx_richspan mulib_richtext_at(const mulib_richtext_t *r, size_t i)
{
    mgx_richspan fallback = {0, r ? r->text.n : 0, 0xffffffff, 0};
    if (!mgx_richtext_storage(r) || !mulib_textbuffer_valid(&r->text) ||
        i >= r->text.n || !mgx_utf8_boundary(r->text.data, r->text.n, i))
        return fallback;
    for (size_t k = r->n; k; k--) {
        mgx_richspan s = r->v[k - 1];
        if (s.start < s.end && i >= s.start && i < s.end &&
            mgx_utf8_boundary(r->text.data, r->text.n, s.start) &&
            mgx_utf8_boundary(r->text.data, r->text.n, s.end)) return s;
    }
    return fallback;
}

static inline void mulib_richtext_free(mulib_richtext_t *r)
{
    if (!r) return;
    mulib_textbuffer_free(&r->text);
    free(r->v);
    memset(r, 0, sizeof *r);
}
#endif
