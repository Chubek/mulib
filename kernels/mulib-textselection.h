/* MuLIB graphical extension: textselection. C11, header-only. */
#ifndef MULIB_TEXTSELECTION_H
#define MULIB_TEXTSELECTION_H
#include "mulib-textcursor.h"
typedef struct { size_t anchor, caret; } mulib_textselection_t;

static inline size_t mulib_textselection_start(mulib_textselection_t s)
{
    return s.anchor < s.caret ? s.anchor : s.caret;
}

static inline size_t mulib_textselection_end(mulib_textselection_t s)
{
    return s.anchor > s.caret ? s.anchor : s.caret;
}

/* Selections use scalar-aligned byte offsets, not glyph or visual positions. */
static inline int mulib_textselection_valid(const mulib_textbuffer_t *b,
                                           mulib_textselection_t s)
{
    return mulib_textbuffer_valid(b) &&
        mgx_utf8_boundary(b->data, b->n, s.anchor) &&
        mgx_utf8_boundary(b->data, b->n, s.caret);
}

/* Invalid selections are rejected without mutation, including collapsed ones
 * pointing into a continuation byte or past the end. */
static inline int mulib_textselection_set(mulib_textselection_t *s,
                                         const mulib_textbuffer_t *b,
                                         size_t anchor, size_t caret)
{
    mulib_textselection_t t = {anchor, caret};
    if (!s || !mulib_textselection_valid(b, t)) return 0;
    *s = t;
    return 1;
}

static inline void mulib_textselection_move(mulib_textselection_t *s,
                                           const mulib_textbuffer_t *b,
                                           int dir, int extend)
{
    if (!s || !dir || !mulib_textselection_valid(b, *s)) return;
    if (!extend && s->anchor != s->caret)
        s->caret = dir < 0 ? mulib_textselection_start(*s) :
                            mulib_textselection_end(*s);
    else
        s->caret = dir < 0 ? mulib_textcursor_prev(b, s->caret) :
                            mulib_textcursor_next(b, s->caret);
    if (!extend) s->anchor = s->caret;
}
#endif
