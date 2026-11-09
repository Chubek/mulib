/** @file mulib-browsertrace.h
 * @brief Caller-owned ring buffer of timestamped browser action events.
 * C11 single-header implementation. Define MULIB_IMPL_BROWSERTRACE in one
 * translation unit.
 */
#ifndef MULIB_BROWSERTRACE_H
#define MULIB_BROWSERTRACE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_browsertrace_event
  {
    uint64_t time_ms;
    uint32_t kind;
    uint32_t detail;
  } mulib_browsertrace_event_t;
  typedef struct mulib_browsertrace
  {
    mulib_browsertrace_event_t events[128];
    size_t next, count;
  } mulib_browsertrace_t;
  /** @brief Copy into a zero-initialized 128-event ring; overwrite oldest when
   * full. Timestamps and event fields are opaque, not validated or sorted. */
  void mulib_browsertrace_push (mulib_browsertrace_t *t,
                                mulib_browsertrace_event_t e);
  /** @brief Borrow zero-based oldest-first event, or NULL out of range. Ring
   * writes may invalidate the event's content. No allocation or
   * synchronization. */
  const mulib_browsertrace_event_t *
  mulib_browsertrace_get (const mulib_browsertrace_t *t, size_t index);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BROWSERTRACE)                                          \
    && !defined(MULIB_BROWSERTRACE_IMPLEMENTED)
#define MULIB_BROWSERTRACE_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  void
  mulib_browsertrace_push (mulib_browsertrace_t *t,
                           mulib_browsertrace_event_t e)
  {
    if (!t)
      return;
    t->events[t->next] = e;
    t->next = (t->next + 1) % 128;
    if (t->count < 128)
      t->count++;
  }
  const mulib_browsertrace_event_t *
  mulib_browsertrace_get (const mulib_browsertrace_t *t, size_t index)
  {
    if (!t || index >= t->count)
      return NULL;
    return &t->events[(t->next + 128 - t->count + index) % 128];
  }

#ifdef __cplusplus
}
#endif
#endif
