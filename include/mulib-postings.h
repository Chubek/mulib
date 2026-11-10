/* MuLIB: Sorted document postings with term frequency, binary search and set-safe
 * updates. C11 single-header; define MULIB_IMPL_POSTINGS once. */
#ifndef MULIB_POSTINGS_H
#define MULIB_POSTINGS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t document;
    uint32_t frequency;
  } mulib_posting_t;
  typedef struct
  {
    mulib_posting_t *items;
    size_t count, capacity;
  } mulib_postings_t;
  int mulib_postings_put (mulib_postings_t *p, uint64_t document, uint32_t frequency);
  int mulib_postings_find (const mulib_postings_t *p, uint64_t document,
                           uint32_t *frequency);
  int mulib_postings_remove (mulib_postings_t *p, uint64_t document);
  void mulib_postings_free (mulib_postings_t *p);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_POSTINGS) && !defined(MULIB_POSTINGS_IMPLEMENTED)
#define MULIB_POSTINGS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  static size_t
  mulib_postings_lower_ (const mulib_postings_t *p, uint64_t id)
  {
    size_t l = 0, r = p->count;
    while (l < r)
      {
        size_t m = l + (r - l) / 2;
        if (p->items[m].document < id)
          l = m + 1;
        else
          r = m;
      }
    return l;
  }
  int
  mulib_postings_put (mulib_postings_t *p, uint64_t id, uint32_t f)
  {
    if (!p || !f)
      return 0;
    size_t i = mulib_postings_lower_ (p, id);
    if (i < p->count && p->items[i].document == id)
      {
        p->items[i].frequency = f;
        return 1;
      }
    if (p->count == p->capacity)
      {
        size_t nc = p->capacity ? p->capacity * 2 : 8;
        if (nc < p->capacity || nc > SIZE_MAX / sizeof (*p->items))
          return 0;
        void *q = realloc (p->items, nc * sizeof (*p->items));
        if (!q)
          return 0;
        p->items = (mulib_posting_t *)q;
        p->capacity = nc;
      }
    memmove (p->items + i + 1, p->items + i, (p->count - i) * sizeof (*p->items));
    p->items[i] = (mulib_posting_t){ id, f };
    p->count++;
    return 1;
  }
  int
  mulib_postings_find (const mulib_postings_t *p, uint64_t id, uint32_t *f)
  {
    if (!p)
      return 0;
    size_t i = mulib_postings_lower_ (p, id);
    if (i >= p->count || p->items[i].document != id)
      return 0;
    if (f)
      *f = p->items[i].frequency;
    return 1;
  }
  int
  mulib_postings_remove (mulib_postings_t *p, uint64_t id)
  {
    if (!p)
      return 0;
    size_t i = mulib_postings_lower_ (p, id);
    if (i >= p->count || p->items[i].document != id)
      return 0;
    memmove (p->items + i, p->items + i + 1, (p->count - i - 1) * sizeof (*p->items));
    p->count--;
    return 1;
  }
  void
  mulib_postings_free (mulib_postings_t *p)
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
