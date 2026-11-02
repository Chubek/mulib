/** @file mulib-embeddingsdb.h
 * @brief Exact linear Euclidean nearest-neighbor search over owned vectors.
 * Define MULIB_IMPL_EMBEDDINGSDB. Zero initialize and set dimension >0 before
 * use. Stable uint64 ids; replace copies before releasing old vector. No ANN,
 * persistence or concurrent access. Distances may be infinity on overflow. */
#ifndef MULIB_EMBEDDINGSDB_H
#define MULIB_EMBEDDINGSDB_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_embeddingsdb_entry
  {
    uint64_t id;
    double *vector;
  } mulib_embeddingsdb_entry_t;
  typedef struct mulib_embeddingsdb
  {
    size_t dimension, count;
    mulib_embeddingsdb_entry_t *entries;
  } mulib_embeddingsdb_t;
  int mulib_embeddingsdb_put (mulib_embeddingsdb_t *db, uint64_t id,
                              const double *vector, size_t dimension);
  int mulib_embeddingsdb_delete (mulib_embeddingsdb_t *db, uint64_t id);
  /** Output unchanged if empty/invalid. Ties choose smallest id. */
  int mulib_embeddingsdb_nearest (const mulib_embeddingsdb_t *db,
                                  const double *query, size_t dimension,
                                  uint64_t *id, double *distance);
  void mulib_embeddingsdb_free (mulib_embeddingsdb_t *db);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EMBEDDINGSDB) && !defined(MULIB_EMBEDDINGSDB_IMPLEMENTED)
#define MULIB_EMBEDDINGSDB_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int
mulib_embeddingsdb_vector_ (const double *v, size_t n)
{
  if (!v || !n || n > SIZE_MAX / sizeof (double))
    return 0;
  for (size_t i = 0; i < n; ++i)
    if (!isfinite (v[i]))
      return 0;
  return 1;
}
int
mulib_embeddingsdb_put (mulib_embeddingsdb_t *db, uint64_t id, const double *v,
                        size_t n)
{
  size_t i;
  double *p;
  mulib_embeddingsdb_entry_t *entries;
  if (!db || n != db->dimension || !mulib_embeddingsdb_vector_ (v, n))
    return 0;
  for (i = 0; i < db->count && db->entries[i].id != id; ++i)
    {
    }
  if (i == db->count && db->count >= SIZE_MAX / sizeof (*entries))
    return 0;
  p = (double *)malloc (n * sizeof (double));
  if (!p)
    return 0;
  memcpy (p, v, n * sizeof (double));
  if (i == db->count)
    {
      entries = (mulib_embeddingsdb_entry_t *)realloc (
          db->entries, (db->count + 1) * sizeof (*entries));
      if (!entries)
        {
          free (p);
          return 0;
        }
      db->entries = entries;
      ++db->count;
      db->entries[i].vector = NULL;
      db->entries[i].id = id;
    }
  free (db->entries[i].vector);
  db->entries[i].vector = p;
  return 1;
}
int
mulib_embeddingsdb_delete (mulib_embeddingsdb_t *db, uint64_t id)
{
  if (!db)
    return 0;
  for (size_t i = 0; i < db->count; ++i)
    if (db->entries[i].id == id)
      {
        free (db->entries[i].vector);
        db->entries[i] = db->entries[--db->count];
        return 1;
      }
  return 0;
}
int
mulib_embeddingsdb_nearest (const mulib_embeddingsdb_t *db, const double *q,
                            size_t n, uint64_t *id, double *distance)
{
  size_t best = 0;
  double score = INFINITY;
  if (!db || !db->count || !id || !distance || n != db->dimension
      || !mulib_embeddingsdb_vector_ (q, n))
    return 0;
  for (size_t i = 0; i < db->count; ++i)
    {
      double d = 0;
      for (size_t j = 0; j < n; ++j)
        d = hypot (d, q[j] - db->entries[i].vector[j]);
      if (i == 0 || d < score
          || (d == score && db->entries[i].id < db->entries[best].id))
        {
          score = d;
          best = i;
        }
    }
  *id = db->entries[best].id;
  *distance = score;
  return 1;
}
void
mulib_embeddingsdb_free (mulib_embeddingsdb_t *db)
{
  if (db)
    {
      for (size_t i = 0; i < db->count; ++i)
        free (db->entries[i].vector);
      free (db->entries);
      memset (db, 0, sizeof (*db));
    }
}
#endif
