/* MuLIB graphical extension: bounded, single-thread-owned event FIFO. */
#ifndef MULIB_GLX_EVENT_H
#define MULIB_GLX_EVENT_H
#include "mulib-glx-core.h"
typedef enum{MGX_EVENT_NONE,MGX_EVENT_CLOSE,MGX_EVENT_RESIZE,MGX_EVENT_MOTION,MGX_EVENT_BUTTON,MGX_EVENT_SCROLL,MGX_EVENT_KEY,MGX_EVENT_TEXT,MGX_EVENT_FOCUS,MGX_EVENT_TOUCH,MGX_EVENT_FRAME}mgx_event_type;
typedef struct{mgx_event_type type;uint64_t time_us;uint32_t window,code,mods;int down;double x,y,dx,dy;char text[32];}mgx_event;
typedef mgx_event mulib_glx_event_t;

#ifndef MULIB_GLX_EVENT_LIMIT
#define MULIB_GLX_EVENT_LIMIT 4096u
#endif
/* Override both hooks consistently in every TU sharing an owning queue. */
#ifndef MULIB_GLX_EVENT_REALLOC
#define MULIB_GLX_EVENT_REALLOC realloc
#endif
#ifndef MULIB_GLX_EVENT_FREE
#define MULIB_GLX_EVENT_FREE free
#endif
typedef struct {
    mgx_event *v;
    size_t n, cap, head;
    size_t limit, dropped; /* limit == 0 selects MULIB_GLX_EVENT_LIMIT. */
    int overflowed;        /* Sticky until acknowledge_loss(), not pop(). */
} mulib_glx_event_queue_t;

/* Text events contain a NUL-terminated, scalar-valid UTF-8 string. */
static inline int mulib_glx_event_text_valid(const char text[32])
{
    size_t i = 0;
    while (i < 32 && text[i]) {
        unsigned char c = (unsigned char)text[i++];
        uint32_t value, minimum;
        unsigned n;
        if (c < 0x80) continue;
        if (c >= 0xc2 && c <= 0xdf) {
            value = c & 31u; minimum = 0x80; n = 1;
        } else if (c >= 0xe0 && c <= 0xef) {
            value = c & 15u; minimum = 0x800; n = 2;
        } else if (c >= 0xf0 && c <= 0xf4) {
            value = c & 7u; minimum = 0x10000; n = 3;
        } else return 0;
        while (n--) {
            if (i >= 32) return 0;
            c = (unsigned char)text[i++];
            if ((c & 0xc0u) != 0x80u) return 0;
            value = (value << 6) | (c & 63u);
        }
        if (value < minimum || value > 0x10ffffu
            || (value >= 0xd800u && value <= 0xdfffu)) return 0;
    }
    return i < 32;
}

static inline int mulib_glx_event_valid(const mgx_event *e)
{
    if (!e || e->type < MGX_EVENT_NONE || e->type > MGX_EVENT_FRAME
        || !isfinite(e->x) || !isfinite(e->y)
        || !isfinite(e->dx) || !isfinite(e->dy)) return 0;
    switch (e->type) {
    case MGX_EVENT_KEY: return e->code < 768 && e->down >= 0 && e->down <= 2;
    case MGX_EVENT_BUTTON: return e->code < 32 && (e->down == 0 || e->down == 1);
    case MGX_EVENT_FOCUS: return e->down == 0 || e->down == 1;
    case MGX_EVENT_TOUCH: return e->code < 32 && (e->down == 0 || e->down == 1);
    case MGX_EVENT_TEXT: return mulib_glx_event_text_valid(e->text);
    case MGX_EVENT_RESIZE:
        return e->x > 0 && e->y > 0 && e->x <= INT_MAX && e->y <= INT_MAX
            && floor(e->x) == e->x && floor(e->y) == e->y;
    default: return 1;
    }
}

static inline size_t mulib_glx_event_count(const mulib_glx_event_queue_t *q)
{
    return q && q->head <= q->n ? q->n - q->head : 0;
}

/* Does not drop pending events. A smaller limit need not shrink allocation. */
static inline int mulib_glx_event_set_limit(mulib_glx_event_queue_t *q, size_t limit)
{
    if (!q || !limit || limit > SIZE_MAX / sizeof(mgx_event)
        || limit < mulib_glx_event_count(q)) {
        errno = EINVAL;
        return 0;
    }
    q->limit = limit;
    return 1;
}

/* Reject newest on exhaustion; never coalesce, overwrite, or reorder edges.
 * ENOBUFS/ENOMEM failures mark loss, so consumers can cancel held input.
 * Existing pending events remain available even after failure. */
static inline int mulib_glx_event_push(mulib_glx_event_queue_t *q, mgx_event e)
{
    size_t count, limit;
    if (!q || q->head > q->n || q->n > q->cap || (q->cap && !q->v)
        || !mulib_glx_event_valid(&e)) {
        errno = EINVAL;
        return 0;
    }
    count = q->n - q->head;
    limit = q->limit ? q->limit : MULIB_GLX_EVENT_LIMIT;
    if (count >= limit || count >= SIZE_MAX / sizeof e) {
        errno = ENOBUFS;
        goto lost;
    }
    if (count == q->cap) {
        size_t cap = q->cap ? q->cap : 8;
        mgx_event *v;
        if (cap <= count) cap = count > SIZE_MAX / 2 ? limit : count * 2;
        if (cap > limit) cap = limit;
        if (cap > SIZE_MAX / sizeof e) cap = SIZE_MAX / sizeof e;
        v = (mgx_event *)MULIB_GLX_EVENT_REALLOC(q->v, cap * sizeof e);
        if (!v) {
            errno = ENOMEM;
            goto lost;
        }
        q->v = v;
        q->cap = cap;
    }
    if (q->n == q->cap && q->head) {
        memmove(q->v, q->v + q->head, count * sizeof e);
        q->head = 0;
        q->n = count;
    }
    q->v[q->n++] = e;
    return 1;
lost:
    q->overflowed = 1;
    if (q->dropped != SIZE_MAX) ++q->dropped;
    return 0;
}

static inline int mulib_glx_event_pop(mulib_glx_event_queue_t *q, mgx_event *e)
{
    if (!q || !e || q->head > q->n || q->n > q->cap || (q->n && !q->v)) {
        errno = EINVAL;
        return 0;
    }
    if (q->head == q->n) return 0;
    *e = q->v[q->head++];
    if (q->head == q->n) q->head = q->n = 0;
    return 1;
}

/* Discard a discontinuous batch without reallocating or acknowledging loss. */
static inline void mulib_glx_event_clear(mulib_glx_event_queue_t *q)
{
    if (q) q->head = q->n = 0;
}

/* Only acknowledge after the application has reconciled/cancelled input. */
static inline void mulib_glx_event_acknowledge_loss(mulib_glx_event_queue_t *q)
{
    if (q) { q->overflowed = 0; q->dropped = 0; }
}

static inline void mulib_glx_event_free(mulib_glx_event_queue_t *q)
{
    if (q) {
        MULIB_GLX_EVENT_FREE(q->v);
        memset(q, 0, sizeof *q);
    }
}
#endif
