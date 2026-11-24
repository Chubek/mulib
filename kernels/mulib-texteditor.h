/* MuLIB graphical extension: texteditor. C11, header-only. */
#ifndef MULIB_TEXTEDITOR_H
#define MULIB_TEXTEDITOR_H
#include "mulib-texthistory.h"
typedef struct {
    mulib_textbuffer_t text;
    mulib_textselection_t selection;
    mulib_texthistory_t history;
} mulib_texteditor_t;

/* Init only into an empty/uninitialized object, never over owned allocations.
 * Failure leaves an empty, freeable editor. Zero limits select defaults. */
static inline int mulib_texteditor_init_with_limits(mulib_texteditor_t *e,
                                                   const char *s, size_t n,
                                                   size_t limit,
                                                   size_t byte_limit)
{
    if (!e) return 0;
    memset(e, 0, sizeof *e);
    /* Reject an impossible/over-budget initial snapshot before reading input. */
    if (n == SIZE_MAX || n + 1 > mgx_texthistory_byte_limit(byte_limit)) return 0;
    e->history.limit = limit;
    e->history.byte_limit = byte_limit;
    if (!mulib_textbuffer_set(&e->text, s, n)) {
        memset(e, 0, sizeof *e);
        return 0;
    }
    e->selection = (mulib_textselection_t){n, n};
    if (!mulib_texthistory_record(&e->history, &e->text, e->selection)) {
        mulib_textbuffer_free(&e->text);
        memset(e, 0, sizeof *e);
        return 0;
    }
    return 1;
}

static inline int mulib_texteditor_init(mulib_texteditor_t *e,
                                        const char *s, size_t n)
{
    return mulib_texteditor_init_with_limits(e, s, n, 0, 0);
}

/* The live text must still match the current history state. Editing the public
 * nested buffer directly would otherwise associate a selection with unrelated
 * snapshot text. Use editor replacement APIs to preserve that relationship. */
static inline int mulib_texteditor_valid(const mulib_texteditor_t *e)
{
    const mulib_textbuffer_t *current;
    if (!e || !mulib_textselection_valid(&e->text, e->selection) ||
        !mgx_texthistory_storage(&e->history) || !e->history.n) return 0;
    current = &e->history.v[e->history.pos].text;
    return mulib_textbuffer_valid(current) && current->n == e->text.n &&
        (!current->n || !memcmp(current->data, e->text.data, current->n));
}

/* Explicit range replacement shares the same atomic transaction as insert.
 * Selection-only motion is captured in the outgoing undo snapshot on success. */
static inline int mulib_texteditor_replace(mulib_texteditor_t *e,
                                           size_t start, size_t end,
                                           const char *s, size_t n)
{
    mulib_textbuffer_t t = {0};
    mulib_textselection_t selection;
    size_t retained;
    if (!mulib_texteditor_valid(e) || start > end || end > e->text.n)
        return 0;
    retained = e->text.n - (end - start);
    if (n > SIZE_MAX - retained - 1 ||
        retained + n + 1 > mgx_texthistory_byte_limit(e->history.byte_limit) ||
        !mgx_utf8_boundary(e->text.data, e->text.n, start) ||
        !mgx_utf8_boundary(e->text.data, e->text.n, end) ||
        !mgx_utf8_valid(s, n)) return 0;
    if (start == end && !n) return 1; /* Do not discard redo for a no-op. */
    if (!mulib_textbuffer_set(&t, e->text.data, e->text.n) ||
        !mulib_textbuffer_replace(&t, start, end, s, n)) {
        mulib_textbuffer_free(&t);
        return 0;
    }
    selection = (mulib_textselection_t){start + n, start + n};
    if (!mulib_texthistory_record(&e->history, &t, selection)) {
        mulib_textbuffer_free(&t);
        return 0;
    }
    if (e->history.n > 1)
        e->history.v[e->history.n - 2].selection = e->selection;
    mulib_textbuffer_free(&e->text);
    e->text = t;
    e->selection = selection;
    return 1;
}

static inline int mulib_texteditor_insert(mulib_texteditor_t *e,
                                          const char *s, size_t n)
{
    return e && mulib_texteditor_replace(e, mulib_textselection_start(e->selection),
                                         mulib_textselection_end(e->selection), s, n);
}

static inline int mulib_texteditor_delete(mulib_texteditor_t *e, int backward)
{
    size_t start, end;
    if (!e || !mulib_textselection_valid(&e->text, e->selection)) return 0;
    start = mulib_textselection_start(e->selection);
    end = mulib_textselection_end(e->selection);
    if (start == end) {
        if (backward) start = mulib_textcursor_prev(&e->text, start);
        else end = mulib_textcursor_next(&e->text, end);
    }
    return mulib_texteditor_replace(e, start, end, NULL, 0);
}

static inline int mulib_texteditor_undo(mulib_texteditor_t *e, int redo)
{
    mulib_textselection_t outgoing;
    size_t pos;
    if (!mulib_texteditor_valid(e)) return 0;
    pos = e->history.pos;
    outgoing = e->selection;
    if (!mulib_textselection_valid(&e->history.v[pos].text, outgoing) ||
        !mulib_texthistory_step(&e->history, &e->text, &e->selection, redo))
        return 0;
    e->history.v[pos].selection = outgoing;
    return 1;
}

static inline void mulib_texteditor_free(mulib_texteditor_t *e)
{
    if (!e) return;
    mulib_textbuffer_free(&e->text);
    mulib_texthistory_free(&e->history);
    memset(e, 0, sizeof *e);
}
#endif
