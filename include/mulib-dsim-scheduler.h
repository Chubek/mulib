/** @file mulib-dsim-scheduler.h
 * @brief Stable min-heap event ordering by time, delta, region and insertion order, with peek and
 * cancellation by signal.
 * @details C11 single-header implementation; define MULIB_IMPL_DSIM_SCHEDULER in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_DSIM_SCHEDULER_H
#define MULIB_DSIM_SCHEDULER_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-dsim-logic4.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_dsim_event {
    uint64_t time, delta, order;
    uint32_t region, signal;
    mulib_dsim_logic4_t value;
} mulib_dsim_event_t;
typedef struct mulib_dsim_scheduler {
    mulib_dsim_event_t *heap;
    size_t count, cap;
    uint64_t next_order;
} mulib_dsim_scheduler_t;
int mulib_dsim_scheduler_push(mulib_dsim_scheduler_t *s, uint64_t time, uint64_t delta,
                              uint32_t region, uint32_t signal, mulib_dsim_logic4_t value);
int mulib_dsim_scheduler_pop(mulib_dsim_scheduler_t *s, mulib_dsim_event_t *out);
void mulib_dsim_scheduler_free(mulib_dsim_scheduler_t *s);
int mulib_dsim_scheduler_peek(const mulib_dsim_scheduler_t *s, mulib_dsim_event_t *out);
size_t mulib_dsim_scheduler_cancel(mulib_dsim_scheduler_t *s, uint32_t signal);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DSIM_SCHEDULER_H */
#if defined(MULIB_IMPL_DSIM_SCHEDULER) && !defined(MULIB_DSIM_SCHEDULER_H_IMPLEMENTED)
#define MULIB_DSIM_SCHEDULER_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_dsim_less_(mulib_dsim_event_t a, mulib_dsim_event_t b) {
    if (a.time != b.time)
        return a.time < b.time;
    if (a.delta != b.delta)
        return a.delta < b.delta;
    if (a.region != b.region)
        return a.region < b.region;
    return a.order < b.order;
}
void mulib_dsim_scheduler_free(mulib_dsim_scheduler_t *s) {
    if (!s)
        return;
    free(s->heap);
    memset(s, 0, sizeof *s);
}
int mulib_dsim_scheduler_push(mulib_dsim_scheduler_t *s, uint64_t time, uint64_t delta,
                              uint32_t reg, uint32_t signal, mulib_dsim_logic4_t v) {
    if (!s || s->count > s->cap || (s->count && !s->heap) || v < MULIB_DSIM_0 || v > MULIB_DSIM_Z ||
        s->next_order == UINT64_MAX)
        return 0;
    if (s->count == s->cap) {
        size_t n = s->cap ? s->cap * 2 : 16;
        if (n <= s->cap || n > SIZE_MAX / sizeof *s->heap)
            return 0;
        void *p = realloc(s->heap, n * sizeof *s->heap);
        if (!p)
            return 0;
        s->heap = p;
        s->cap = n;
    }
    size_t i = s->count++;
    mulib_dsim_event_t e = {time, delta, s->next_order++, reg, signal, v};
    while (i) {
        size_t p = (i - 1) / 2;
        if (!mulib_dsim_less_(e, s->heap[p]))
            break;
        s->heap[i] = s->heap[p];
        i = p;
    }
    s->heap[i] = e;
    return 1;
}
int mulib_dsim_scheduler_pop(mulib_dsim_scheduler_t *s, mulib_dsim_event_t *out) {
    if (!s || !out || !s->count || s->count > s->cap || !s->heap)
        return 0;
    *out = s->heap[0];
    mulib_dsim_event_t tail = s->heap[--s->count];
    if (!s->count)
        return 1;
    size_t i = 0;
    for (;;) {
        size_t l = 2 * i + 1, r = l + 1;
        if (l >= s->count)
            break;
        size_t c = (r < s->count && mulib_dsim_less_(s->heap[r], s->heap[l])) ? r : l;
        if (!mulib_dsim_less_(s->heap[c], tail))
            break;
        s->heap[i] = s->heap[c];
        i = c;
    }
    s->heap[i] = tail;
    return 1;
}
int mulib_dsim_scheduler_peek(const mulib_dsim_scheduler_t *s, mulib_dsim_event_t *out) {
    if (!s || !out || !s->count || s->count > s->cap || !s->heap)
        return 0;
    *out = s->heap[0];
    return 1;
}
size_t mulib_dsim_scheduler_cancel(mulib_dsim_scheduler_t *s, uint32_t signal) {
    if (!s || s->count > s->cap || (s->count && !s->heap))
        return 0;
    size_t old = s->count, n = 0;
    for (size_t i = 0; i < old; i++)
        if (s->heap[i].signal != signal)
            s->heap[n++] = s->heap[i];
    s->count = n;
    /* Rebuild using the existing order keys, preserving FIFO ties. */
    for (size_t k = n / 2; k > 0; k--) {
        size_t i = k - 1;
        mulib_dsim_event_t e = s->heap[i];
        while (i < n / 2) {
            size_t child = 2 * i + 1;
            if (child + 1 < n && mulib_dsim_less_(s->heap[child + 1], s->heap[child]))
                child++;
            if (!mulib_dsim_less_(s->heap[child], e))
                break;
            s->heap[i] = s->heap[child];
            i = child;
        }
        s->heap[i] = e;
    }
    return old - n;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_DSIM_SCHEDULER */
