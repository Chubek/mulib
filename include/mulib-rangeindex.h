/* MuLIB: Mutable interval index with exact inclusive point and overlap queries. C11
 * single-header; define MULIB_IMPL_RANGEINDEX once. */
#ifndef MULIB_RANGEINDEX_H
#define MULIB_RANGEINDEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    int64_t low, high;
    uint64_t id;
  } mulib_rangeindex_entry_t;
  typedef struct
  {
    mulib_rangeindex_entry_t *entries;
    size_t count, capacity;
  } mulib_rangeindex_t;
  int mulib_rangeindex_insert (mulib_rangeindex_t *idx, int64_t low, int64_t high,
                               uint64_t id);
  /* Linear exact scan, callback returns nonzero to continue; count includes stop. */
  typedef int (*mulib_rangeindex_visit_fn) (uint64_t id, void *context);
  size_t mulib_rangeindex_overlap (const mulib_rangeindex_t *idx, int64_t low,
                                   int64_t high, mulib_rangeindex_visit_fn visit,
                                   void *context);
  void mulib_rangeindex_free (mulib_rangeindex_t *idx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RANGEINDEX) && !defined(MULIB_RANGEINDEX_IMPLEMENTED)
#define MULIB_RANGEINDEX_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_rangeindex_insert (mulib_rangeindex_t *t, int64_t low, int64_t high,
                           uint64_t id)
  {
    if (!t || low > high)
      return 0;
    if (t->count == t->capacity)
      {
        size_t cap = t->capacity ? t->capacity * 2 : 8;
        if (cap < t->capacity || cap > SIZE_MAX / sizeof (*t->entries))
          return 0;
        void *p = realloc (t->entries, cap * sizeof (*t->entries));
        if (!p)
          return 0;
        t->entries = (mulib_rangeindex_entry_t *)p;
        t->capacity = cap;
      }
    size_t i = 0;
    while (i < t->count && t->entries[i].low <= low)
      i++;
    memmove (t->entries + i + 1, t->entries + i, (t->count - i) * sizeof (*t->entries));
    t->entries[i] = (mulib_rangeindex_entry_t){ low, high, id };
    t->count++;
    return 1;
  }
  size_t
  mulib_rangeindex_overlap (const mulib_rangeindex_t *t, int64_t low, int64_t high,
                            mulib_rangeindex_visit_fn fn, void *ctx)
  {
    if (!t || !fn || low > high)
      return SIZE_MAX;
    size_t n = 0;
    for (size_t i = 0; i < t->count; i++)
      {
        if (t->entries[i].low > high)
          break;
        if (t->entries[i].high >= low)
          {
            n++;
            if (!fn (t->entries[i].id, ctx))
              break;
          }
      }
    return n;
  }
  void
  mulib_rangeindex_free (mulib_rangeindex_t *t)
  {
    if (!t)
      return;
    free (t->entries);
    memset (t, 0, sizeof (*t));
  }
#ifdef __cplusplus
}
#endif
#endif
