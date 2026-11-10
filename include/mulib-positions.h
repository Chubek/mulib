/* MuLIB: Owned sorted per-document token positions for phrase matching. C11
 * single-header; define MULIB_IMPL_POSITIONS once. */
#ifndef MULIB_POSITIONS_H
#define MULIB_POSITIONS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint32_t *items;
    size_t count, capacity;
  } mulib_positions_t;
  int mulib_positions_add (mulib_positions_t *p, uint32_t token_position);
  void mulib_positions_free (mulib_positions_t *p);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_POSITIONS) && !defined(MULIB_POSITIONS_IMPLEMENTED)
#define MULIB_POSITIONS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_positions_add (mulib_positions_t *p, uint32_t x)
  {
    if (!p)
      return 0;
    size_t lo = 0, hi = p->count;
    while (lo < hi)
      {
        size_t m = lo + (hi - lo) / 2;
        if (p->items[m] < x)
          lo = m + 1;
        else
          hi = m;
      }
    if (lo < p->count && p->items[lo] == x)
      return 1;
    if (p->count == p->capacity)
      {
        size_t cap = p->capacity ? p->capacity * 2 : 8;
        if (cap < p->capacity || cap > SIZE_MAX / sizeof (uint32_t))
          return 0;
        void *q = realloc (p->items, cap * sizeof (uint32_t));
        if (!q)
          return 0;
        p->items = (uint32_t *)q;
        p->capacity = cap;
      }
    memmove (p->items + lo + 1, p->items + lo, (p->count - lo) * sizeof (uint32_t));
    p->items[lo] = x;
    p->count++;
    return 1;
  }
  void
  mulib_positions_free (mulib_positions_t *p)
  {
    if (!p)
      return;
    free (p->items);
    memset (p, 0, sizeof (*p));
  }
#ifdef __cplusplus
}
#endif
#endif
