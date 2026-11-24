/* MuLIB graphical extension: texthistory. C11, header-only. */
#ifndef MULIB_TEXTHISTORY_H
#define MULIB_TEXTHISTORY_H
#include "mulib-textselection.h"
#ifndef MULIB_TEXTHISTORY_DEFAULT_LIMIT
#define MULIB_TEXTHISTORY_DEFAULT_LIMIT 64
#endif
#ifndef MULIB_TEXTHISTORY_DEFAULT_BYTE_LIMIT
#define MULIB_TEXTHISTORY_DEFAULT_BYTE_LIMIT ((size_t)8 * 1024 * 1024)
#endif

typedef struct {
    mulib_textbuffer_t text;
    mulib_textselection_t selection;
} mgx_text_snapshot;
typedef mgx_text_snapshot mulib_text_snapshot_t;

typedef struct {
    mgx_text_snapshot *v;
    size_t n, cap, pos, limit;
    /* Allocated snapshot text bytes, including one terminator per snapshot.
     * Zero limits select the defaults above; metadata is count-bounded. */
    size_t bytes, byte_limit;
} mulib_texthistory_t;

static inline size_t mgx_texthistory_limit(size_t limit)
{
    size_t n = limit ? limit : MULIB_TEXTHISTORY_DEFAULT_LIMIT;
    return n ? n : 1;
}

static inline size_t mgx_texthistory_byte_limit(size_t limit)
{
    size_t n = limit ? limit : MULIB_TEXTHISTORY_DEFAULT_BYTE_LIMIT;
    return n ? n : 1;
}

static inline int mgx_texthistory_storage(const mulib_texthistory_t *h)
{
    return h && h->n <= h->cap && h->cap <= SIZE_MAX / sizeof *h->v &&
        (h->v ? h->cap != 0 : h->cap == 0) &&
        (h->n ? h->pos < h->n : h->pos == 0);
}

/* Calculate from actual capacities rather than trusting a caller-written bytes
 * counter. Snapshot buffers allocated by record have exactly n + 1 bytes. */
static inline int mgx_texthistory_bytes(const mulib_texthistory_t *h,
                                       size_t end, size_t *bytes)
{
    size_t total = 0;
    for (size_t i = 0; i < end; i++) {
        const mulib_textbuffer_t *b = &h->v[i].text;
        if (!mgx_textbuffer_storage(b) || !b->data ||
            b->cap > SIZE_MAX - total) return 0;
        total += b->cap;
    }
    *bytes = total;
    return 1;
}

/* The new state must fit by itself. Otherwise failure is atomic, including the
 * redo branch. Successful records evict oldest undo states until both caps fit. */
static inline int mulib_texthistory_record(mulib_texthistory_t *h,
                                          const mulib_textbuffer_t *b,
                                          mulib_textselection_t s)
{
    mgx_text_snapshot t = {0};
    size_t keep, drop = 0, bytes, cost, count_limit, byte_limit, count;
    if (!mgx_texthistory_storage(h) || !mulib_textselection_valid(b, s))
        return 0;
    cost = b->n + 1;
    count_limit = mgx_texthistory_limit(h->limit);
    byte_limit = mgx_texthistory_byte_limit(h->byte_limit);
    if (cost > byte_limit) return 0;
    keep = h->n ? h->pos + 1 : 0;
    if (!mgx_texthistory_bytes(h, keep, &bytes)) return 0;
    while (drop < keep &&
           (keep - drop >= count_limit || bytes > byte_limit - cost)) {
        bytes -= h->v[drop].text.cap;
        drop++;
    }
    count = keep - drop + 1;
    if (count > SIZE_MAX / sizeof *h->v) return 0;
    t.text.data = (char *)malloc(cost);
    if (!t.text.data) return 0;
    if (b->n) memcpy(t.text.data, b->data, b->n);
    t.text.data[b->n] = 0;
    t.text.n = b->n;
    t.text.cap = cost;
    t.selection = s;
    if (!mgx_grow((void **)&h->v, &h->cap, count, sizeof *h->v)) {
        mulib_textbuffer_free(&t.text);
        return 0;
    }
    /* No fallible operations after here. b may refer to a retained snapshot. */
    for (size_t i = keep; i < h->n; i++) mulib_textbuffer_free(&h->v[i].text);
    for (size_t i = 0; i < drop; i++) mulib_textbuffer_free(&h->v[i].text);
    if (keep > drop)
        memmove(h->v, h->v + drop, (keep - drop) * sizeof *h->v);
    h->v[count - 1] = t;
    h->n = count;
    h->pos = count - 1;
    h->bytes = bytes + cost;
    return 1;
}

/* Apply caps immediately, preserving the current snapshot. Trim farthest redo
 * first, then oldest undo. A current snapshot exceeding the budget rejects the
 * entire configuration change. This function does not allocate. */
static inline int mulib_texthistory_set_limits(mulib_texthistory_t *h,
                                              size_t limit, size_t byte_limit)
{
    size_t start = 0, end, bytes, counts = mgx_texthistory_limit(limit);
    size_t budget = mgx_texthistory_byte_limit(byte_limit);
    if (!mgx_texthistory_storage(h) ||
        !mgx_texthistory_bytes(h, h->n, &bytes)) return 0;
    if (h->n && h->v[h->pos].text.cap > budget) return 0;
    end = h->n;
    while (end - start > counts || bytes > budget) {
        if (end > h->pos + 1) {
            end--;
            bytes -= h->v[end].text.cap;
        } else {
            bytes -= h->v[start].text.cap;
            start++;
        }
    }
    for (size_t i = 0; i < start; i++) mulib_textbuffer_free(&h->v[i].text);
    for (size_t i = end; i < h->n; i++) mulib_textbuffer_free(&h->v[i].text);
    if (end > start)
        memmove(h->v, h->v + start, (end - start) * sizeof *h->v);
    h->n = end - start;
    h->pos = h->n ? h->pos - start : 0;
    h->bytes = bytes;
    h->limit = limit;
    h->byte_limit = byte_limit;
    return 1;
}

static inline int mulib_texthistory_step(mulib_texthistory_t *h,
                                        mulib_textbuffer_t *b,
                                        mulib_textselection_t *s, int redo)
{
    size_t p;
    mulib_textselection_t selection;
    if (!s || !mgx_texthistory_storage(h) || !h->n ||
        (redo ? h->pos == h->n - 1 : h->pos == 0)) return 0;
    p = redo ? h->pos + 1 : h->pos - 1;
    selection = h->v[p].selection;
    if (!mulib_textselection_valid(&h->v[p].text, selection) ||
        !mulib_textbuffer_set(b, h->v[p].text.data, h->v[p].text.n)) return 0;
    *s = selection;
    h->pos = p;
    return 1;
}

static inline void mulib_texthistory_free(mulib_texthistory_t *h)
{
    if (!h) return;
    for (size_t i = 0; i < h->n; i++) mulib_textbuffer_free(&h->v[i].text);
    free(h->v);
    memset(h, 0, sizeof *h);
}
#endif
