/** @file mulib-webtimers.h
 * @brief Caller-clocked fixed-capacity timer scheduler.
 * C11 single-header implementation. Define MULIB_IMPL_WEBTIMERS in one
 * translation unit.
 */
#ifndef MULIB_WEBTIMERS_H
#define MULIB_WEBTIMERS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_webtimer
  {
    uint64_t due_ms, id;
  } mulib_webtimer_t;
  typedef struct mulib_webtimers
  {
    mulib_webtimer_t values[64];
    size_t count;
    uint64_t next_id;
  } mulib_webtimers_t;
  /** @brief Start zeroed; at most 64 one-shot timers. Return nonzero unique ID
   * or 0 (capacity, deadline overflow, ID exhaustion); unchanged on failure.
   */
  uint64_t mulib_webtimers_add (mulib_webtimers_t *t, uint64_t now,
                                uint64_t delay);
  /** @brief Remove an ID; 1 removed, 0 unknown/invalid. */
  int mulib_webtimers_cancel (mulib_webtimers_t *t, uint64_t id);
  /** @brief Consume earliest due timer; equal deadlines use insertion order.
   * Return ID or 0 if none due. Caller supplies monotonic millisecond time. */
  uint64_t mulib_webtimers_next (mulib_webtimers_t *t, uint64_t now);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBTIMERS) && !defined(MULIB_WEBTIMERS_IMPLEMENTED)
#define MULIB_WEBTIMERS_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  uint64_t
  mulib_webtimers_add (mulib_webtimers_t *t, uint64_t now, uint64_t delay)
  {
    if (!t || t->count >= 64 || UINT64_MAX - now < delay
        || t->next_id == UINT64_MAX)
      return 0;
    uint64_t id = ++t->next_id;
    t->values[t->count].due_ms = now + delay;
    t->values[t->count++].id = id;
    return id;
  }
  int
  mulib_webtimers_cancel (mulib_webtimers_t *t, uint64_t id)
  {
    if (!t || !id)
      return 0;
    for (size_t i = 0; i < t->count; i++)
      if (t->values[i].id == id)
        {
          t->values[i] = t->values[--t->count];
          return 1;
        }
    return 0;
  }
  uint64_t
  mulib_webtimers_next (mulib_webtimers_t *t, uint64_t now)
  {
    if (!t)
      return 0;
    size_t pick = t->count;
    for (size_t i = 0; i < t->count; i++)
      if (t->values[i].due_ms <= now
          && (pick == t->count || t->values[i].due_ms < t->values[pick].due_ms
              || (t->values[i].due_ms == t->values[pick].due_ms
                  && t->values[i].id < t->values[pick].id)))
        pick = i;
    if (pick == t->count)
      return 0;
    uint64_t id = t->values[pick].id;
    t->values[pick] = t->values[--t->count];
    return id;
  }

#ifdef __cplusplus
}
#endif
#endif
