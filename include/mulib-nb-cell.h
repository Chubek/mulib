/* SPDX-License-Identifier: MIT
 * Bounded notebook text. Link the upstream UTF-8 kernel in one translation unit. */
#ifndef MULIB_NB_CELL_H
#define MULIB_NB_CELL_H
#include "mulib-wm-core.h"
#include "mulib-utf8.h"

#define MULIB_NB_TEXT_MAX ((size_t)16 * 1024 * 1024)
#define MULIB_NB_LABEL_MAX ((size_t)4096)
#define MULIB_NB_DEFAULT_BYTES ((size_t)64 * 1024 * 1024)
#define MULIB_NB_DEFAULT_CELLS ((size_t)1000000)

typedef enum {
    MULIB_NB_MARKDOWN = 1, MULIB_NB_CODE, MULIB_NB_APP,
    MULIB_NB_TERMINAL, MULIB_NB_MEDIA, MULIB_NB_GROUP, MULIB_NB_RAW
} mulib_nb_kind;

typedef struct {
    uint64_t id, parent, window, revision;
    mulib_nb_kind kind;
    double height;
    unsigned flags;
    char *source, *language;
} mulib_nb_cell;

enum { MULIB_NB_COLLAPSED = 1, MULIB_NB_TRUSTED = 2 };

/* All lengths are bytes, excluding the terminator. Embedded NUL is forbidden. */
static inline int
mulib_nb_text_valid(const char *s, size_t n, size_t limit)
{
    return s && n <= limit && !memchr(s, 0, n)
        && mulib_utf8_validate((const unsigned char *)s, n);
}

static inline int
mulib_nb_text_length(const char *s, size_t limit, size_t *length)
{
    if (!s || !length)
        return 0;
    size_t n = 0;
    while (n < limit && s[n])
        ++n;
    if (s[n] || !mulib_nb_text_valid(s, n, limit))
        return 0;
    *length = n;
    return 1;
}

static inline char *
mulib_nb_text_copy(const char *s, size_t n)
{
    if (n == SIZE_MAX)
        return NULL;
    char *p = (char *)malloc(n + 1);
    if (p) {
        if (n)
            memcpy(p, s, n);
        p[n] = 0;
    }
    return p;
}

static inline void
mulib_nb_cell_free(mulib_nb_cell *c)
{
    if (c) {
        free(c->source);
        free(c->language);
        memset(c, 0, sizeof *c);
    }
}

/* Failure leaves text, revision and trust unchanged, including aliased input. */
static inline int
mulib_nb_cell_text_n(mulib_nb_cell *c, const char *src, size_t ns,
                     const char *lang, size_t nl)
{
    if (!c || c->revision == UINT64_MAX
        || !mulib_nb_text_valid(src, ns, MULIB_NB_TEXT_MAX)
        || !mulib_nb_text_valid(lang, nl, MULIB_NB_LABEL_MAX))
        return 0;
    char *s = mulib_nb_text_copy(src, ns);
    char *l = mulib_nb_text_copy(lang, nl);
    if (!s || !l) {
        free(s);
        free(l);
        return 0;
    }
    free(c->source);
    free(c->language);
    c->source = s;
    c->language = l;
    ++c->revision;
    c->flags &= ~MULIB_NB_TRUSTED;
    return 1;
}

static inline int
mulib_nb_cell_text(mulib_nb_cell *c, const char *src, const char *lang)
{
    size_t ns, nl;
    return mulib_nb_text_length(src, MULIB_NB_TEXT_MAX, &ns)
        && mulib_nb_text_length(lang, MULIB_NB_LABEL_MAX, &nl)
        && mulib_nb_cell_text_n(c, src, ns, lang, nl);
}
#endif
