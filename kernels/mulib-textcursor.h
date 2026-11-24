/* MuLIB graphical extension: textcursor. C11, header-only. */
#ifndef MULIB_TEXTCURSOR_H
#define MULIB_TEXTCURSOR_H
#include "mulib-textbuffer.h"
/* Deliberately limited grapheme approximation, not Unicode UAX #29 or BiDi. */
static inline int mgx_extend(uint32_t c)
{
    return (c >= 0x300 && c <= 0x36f) || (c >= 0x1ab0 && c <= 0x1aff) ||
        (c >= 0x1dc0 && c <= 0x1dff) || (c >= 0xfe00 && c <= 0xfe0f) ||
        (c >= 0x1f3fb && c <= 0x1f3ff);
}

/* Internal: b is valid and pos is a scalar boundary. */
static inline size_t mgx_textcursor_next(const mulib_textbuffer_t *b, size_t pos)
{
    uint32_t a, c;
    size_t k;
    int ri;
    if (pos >= b->n) return b->n;
    if (!mgx_utf8_decode(b->data + pos, b->n - pos, &a, &k)) return pos;
    pos += k;
    ri = a >= 0x1f1e6 && a <= 0x1f1ff;
    while (pos < b->n && mgx_utf8_decode(b->data + pos, b->n - pos, &c, &k)) {
        if (!mgx_extend(c) && c != 0x200d && a != 0x200d &&
            !(a == 13 && c == 10) && !(ri && c >= 0x1f1e6 && c <= 0x1f1ff))
            break;
        pos += k;
        a = c;
        /* Combining marks do not consume a regional-indicator pair. */
        if (!mgx_extend(c)) ri = 0;
    }
    return pos;
}

/* Invalid buffers return zero. Oversized offsets clamp to the end. Interior
 * byte offsets move to the enclosing approximate cluster's next/previous edge.
 * Scanning from the start keeps RI pairing and next/prev boundaries consistent. */
static inline size_t mulib_textcursor_next(const mulib_textbuffer_t *b, size_t pos)
{
    size_t i = 0;
    if (!mulib_textbuffer_valid(b)) return 0;
    if (pos >= b->n) return b->n;
    while (i <= pos) {
        size_t next = mgx_textcursor_next(b, i);
        if (next <= i) return i;
        i = next;
    }
    return i;
}

static inline size_t mulib_textcursor_prev(const mulib_textbuffer_t *b, size_t pos)
{
    size_t i = 0, prior = 0;
    if (!mulib_textbuffer_valid(b)) return 0;
    if (pos > b->n) pos = b->n;
    while (i < pos) {
        size_t next = mgx_textcursor_next(b, i);
        if (next <= i) break;
        prior = i;
        i = next;
    }
    return prior;
}
#endif
