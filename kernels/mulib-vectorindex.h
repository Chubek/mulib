/* MuLIB: Owned exact single-nearest-neighbor index over finite double vectors.
 * IDs may be replaced. C11 single-header; define MULIB_IMPL_VECTORINDEX once. */
#ifndef MULIB_VECTORINDEX_H
#define MULIB_VECTORINDEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t id;
    double *values;
  } mulib_vectorindex_entry_t;
  typedef struct
  {
    size_t dimension, count, capacity;
    mulib_vectorindex_entry_t *entries;
  } mulib_vectorindex_t;
  int mulib_vectorindex_insert (mulib_vectorindex_t *idx, uint64_t id,
                                const double *values, size_t dimension);
  /* Return nearest id, 1 success, 0 absent/invalid. */
  int mulib_vectorindex_nearest (const mulib_vectorindex_t *idx, const double *query,
                                 size_t dimension, uint64_t *id, double *distance);
  void mulib_vectorindex_free (mulib_vectorindex_t *idx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VECTORINDEX) && !defined(MULIB_VECTORINDEX_IMPLEMENTED)
#define MULIB_VECTORINDEX_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_vectorindex_insert (mulib_vectorindex_t *t, uint64_t id, const double *v,
                            size_t n)
  {
    if (!t || !v || !n || n != t->dimension || n > SIZE_MAX / sizeof (double))
      return 0;
    for (size_t i = 0; i < n; i++)
      if (!isfinite (v[i]))
        return 0;
    double *copy = (double *)malloc (n * sizeof (double));
    if (!copy)
      return 0;
    memcpy (copy, v, n * sizeof (double));
    for (size_t i = 0; i < t->count; i++)
      if (t->entries[i].id == id)
        {
          free (t->entries[i].values);
          t->entries[i].values = copy;
          return 1;
        }
    if (t->count == t->capacity)
      {
        size_t cap = t->capacity ? t->capacity * 2 : 8;
        if (cap < t->capacity || cap > SIZE_MAX / sizeof (*t->entries))
          {
            free (copy);
            return 0;
          }
        void *p = realloc (t->entries, cap * sizeof (*t->entries));
        if (!p)
          {
            free (copy);
            return 0;
          }
        t->entries = (mulib_vectorindex_entry_t *)p;
        t->capacity = cap;
      }
    t->entries[t->count++] = (mulib_vectorindex_entry_t){ id, copy };
    return 1;
  }
  int
  mulib_vectorindex_nearest (const mulib_vectorindex_t *t, const double *q, size_t n,
                             uint64_t *id, double *dist)
  {
    if (!t || !q || !id || !dist || !t->count || n != t->dimension)
      return 0;
    for (size_t j = 0; j < n; j++)
      if (!isfinite (q[j]))
        return 0;
    double best = INFINITY;
    uint64_t chosen = 0;
    for (size_t i = 0; i < t->count; i++)
      {
        double d = 0;
        for (size_t j = 0; j < n; j++)
          d = hypot (d, q[j] - t->entries[i].values[j]);
        if (i == 0 || d < best || (d == best && t->entries[i].id < chosen))
          {
            best = d;
            chosen = t->entries[i].id;
          }
      }
    *id = chosen;
    *dist = best;
    return 1;
  }
  void
  mulib_vectorindex_free (mulib_vectorindex_t *t)
  {
    if (!t)
      return;
    for (size_t i = 0; i < t->count; i++)
      free (t->entries[i].values);
    free (t->entries);
    memset (t, 0, sizeof (*t));
  }
#ifdef __cplusplus
}
#endif
#endif
