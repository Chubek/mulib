/* MuLIB graphical extension: textbuffer. C11, header-only. */
#ifndef MULIB_TEXTBUFFER_H
#define MULIB_TEXTBUFFER_H
#include "mulib-glx-core.h"
typedef struct {
    char *data;
    size_t n, cap;
} mulib_textbuffer_t;

/* Decode one Unicode scalar. Failure leaves both output arguments unchanged. */
static inline int mgx_utf8_decode(const char *s, size_t n,
                                  uint32_t *cp, size_t *used)
{
    unsigned a, k;
    uint32_t c;
    if (!s || !n || !cp || !used) return 0;
    a = (unsigned char)s[0];
    k = a < 128 ? 1 : a >= 194 && a <= 223 ? 2 :
        a >= 224 && a <= 239 ? 3 : a >= 240 && a <= 244 ? 4 : 0;
    if (!k || k > n) return 0;
    c = k == 1 ? a : a & ((1u << (7 - k)) - 1);
    for (unsigned i = 1; i < k; i++) {
        unsigned b = (unsigned char)s[i];
        if ((b & 192) != 128) return 0;
        c = (c << 6) | (b & 63);
    }
    if ((k == 2 && c < 128) || (k == 3 && c < 2048) ||
        (k == 4 && c < 65536) || c > 0x10ffff ||
        (c >= 0xd800 && c <= 0xdfff)) return 0;
    *cp = c;
    *used = k;
    return 1;
}

static inline int mgx_utf8_valid(const char *s, size_t n)
{
    size_t k;
    uint32_t c;
    if (!s && n) return 0;
    while (n) {
        if (!mgx_utf8_decode(s, n, &c, &k)) return 0;
        s += k;
        n -= k;
    }
    return 1;
}

/* A byte-boundary query, not a UTF-8 validator: s must already be validated. */
static inline int mgx_utf8_boundary(const char *s, size_t n, size_t i)
{
    return (!n || s) && i <= n &&
        (i == 0 || i == n || ((unsigned char)s[i] & 192) != 128);
}

static inline int mgx_textbuffer_storage(const mulib_textbuffer_t *b)
{
    return b && (b->data ? b->n < b->cap : !b->n && !b->cap);
}

static inline int mulib_textbuffer_valid(const mulib_textbuffer_t *b)
{
    return mgx_textbuffer_storage(b) &&
        (!b->data || b->data[b->n] == 0) && mgx_utf8_valid(b->data, b->n);
}

/* Owned storage, half-open byte ranges. Input may alias any part of b->data.
 * Invalid input, size overflow and allocation failure leave b unchanged. */
static inline int mulib_textbuffer_replace(mulib_textbuffer_t *b,
                                           size_t start, size_t end,
                                           const char *text, size_t n)
{
    size_t retained, len;
    char *copy = NULL;
    if (!mgx_textbuffer_storage(b) || start > end || end > b->n) return 0;
    retained = b->n - (end - start);
    /* Check before reading text: impossible lengths must not be scanned. */
    if (n > SIZE_MAX - retained - 1 || !mulib_textbuffer_valid(b) ||
        !mgx_utf8_boundary(b->data, b->n, start) ||
        !mgx_utf8_boundary(b->data, b->n, end) ||
        !mgx_utf8_valid(text, n)) return 0;
    if (n) {
        copy = (char *)malloc(n);
        if (!copy) return 0;
        memcpy(copy, text, n);
    }
    len = retained + n;
    if (!mgx_grow((void **)&b->data, &b->cap, len + 1, 1)) {
        free(copy);
        return 0;
    }
    memmove(b->data + start + n, b->data + end, b->n - end);
    if (n) memcpy(b->data + start, copy, n);
    free(copy);
    b->n = len;
    b->data[len] = 0;
    return 1;
}

static inline int mulib_textbuffer_set(mulib_textbuffer_t *b,
                                       const char *s, size_t n)
{
    return b && mulib_textbuffer_replace(b, 0, b->n, s, n);
}

static inline void mulib_textbuffer_free(mulib_textbuffer_t *b)
{
    if (!b) return;
    free(b->data);
    memset(b, 0, sizeof *b);
}
#endif
