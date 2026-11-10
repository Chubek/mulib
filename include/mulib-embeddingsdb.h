/* MuLIB embeddingsdb: patched exact metric-aware k-nearest search.
 * Existing struct and public API preserved. */
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
  int mulib_embeddingsdb_nearest (const mulib_embeddingsdb_t *db, const double *query,
                                  size_t dimension, uint64_t *id, double *distance);
  void mulib_embeddingsdb_free (mulib_embeddingsdb_t *db);
  typedef enum
  {
    MULIB_EMBEDDINGS_EUCLIDEAN,
    MULIB_EMBEDDINGS_COSINE,
    MULIB_EMBEDDINGS_DOT
  } mulib_embeddings_metric_t;
  typedef struct
  {
    uint64_t id;
    double distance;
  } mulib_embeddings_hit_t;
  /* Ascending distance; cosine distance = 1-cosine; dot distance = -dot.
   * Invalid input returns SIZE_MAX; on failure outputs remain unchanged. */
  size_t mulib_embeddingsdb_knn (const mulib_embeddingsdb_t *db, const double *query,
                                 size_t dimension, mulib_embeddings_metric_t metric,
                                 size_t k, mulib_embeddings_hit_t *hits);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EMBEDDINGSDB) && !defined(MULIB_EMBEDDINGSDB_IMPLEMENTED)
#define MULIB_EMBEDDINGSDB_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_embeddingsdb_vector_ (const double *v, size_t n)
  {
    if (!v || !n || n > SIZE_MAX / sizeof (double))
      return 0;
    for (size_t i = 0; i < n; i++)
      if (!isfinite (v[i]))
        return 0;
    return 1;
  }
  int
  mulib_embeddingsdb_put (mulib_embeddingsdb_t *db, uint64_t id, const double *v,
                          size_t n)
  {
    if (!db || n != db->dimension || !mulib_embeddingsdb_vector_ (v, n))
      return 0;
    size_t i = 0;
    while (i < db->count && db->entries[i].id != id)
      i++;
    if (i == db->count && db->count >= SIZE_MAX / sizeof (*db->entries))
      return 0;
    double *copy = (double *)malloc (n * sizeof (double));
    if (!copy)
      return 0;
    memcpy (copy, v, n * sizeof (double));
    if (i == db->count)
      {
        mulib_embeddingsdb_entry_t *p = (mulib_embeddingsdb_entry_t *)realloc (
            db->entries, (db->count + 1) * sizeof (*p));
        if (!p)
          {
            free (copy);
            return 0;
          }
        db->entries = p;
        db->entries[i].id = id;
        db->entries[i].vector = NULL;
        db->count++;
      }
    free (db->entries[i].vector);
    db->entries[i].vector = copy;
    return 1;
  }
  int
  mulib_embeddingsdb_delete (mulib_embeddingsdb_t *db, uint64_t id)
  {
    if (!db)
      return 0;
    for (size_t i = 0; i < db->count; i++)
      if (db->entries[i].id == id)
        {
          free (db->entries[i].vector);
          db->entries[i] = db->entries[--db->count];
          return 1;
        }
    return 0;
  }
  int
  mulib_embeddingsdb_nearest (const mulib_embeddingsdb_t *db, const double *q, size_t n,
                              uint64_t *id, double *distance)
  {
    if (!db || !db->count || !id || !distance || n != db->dimension
        || !mulib_embeddingsdb_vector_ (q, n))
      return 0;
    size_t best = 0;
    double score = INFINITY;
    for (size_t i = 0; i < db->count; i++)
      {
        double d = 0;
        for (size_t j = 0; j < n; j++)
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
  static int
  mulib_embeddingsdb_metric_ (const double *a, const double *b, size_t n,
                              mulib_embeddings_metric_t m, double *score)
  {
    double sa = 0, sb = 0;
    long double da = 0, db = 0, dot = 0, euclid = 0;
    if (m == MULIB_EMBEDDINGS_COSINE)
      {
        for (size_t j = 0; j < n; j++)
          {
            sa = fmax (sa, fabs (a[j]));
            sb = fmax (sb, fabs (b[j]));
          }
        if (sa == 0 || sb == 0)
          return 0;
      }
    for (size_t j = 0; j < n; j++)
      {
        if (m == MULIB_EMBEDDINGS_EUCLIDEAN)
          euclid = hypotl (euclid, (long double)a[j] - b[j]);
        else if (m == MULIB_EMBEDDINGS_DOT)
          dot += (long double)a[j] * b[j];
        else
          {
            long double x = (long double)a[j] / sa, y = (long double)b[j] / sb;
            dot += x * y;
            da += x * x;
            db += y * y;
          }
      }
    double d = m == MULIB_EMBEDDINGS_EUCLIDEAN ? (double)euclid
               : m == MULIB_EMBEDDINGS_DOT
                   ? -(double)dot
                   : (double)(1 - fmaxl (-1, fminl (1, dot / sqrtl (da) / sqrtl (db))));
    if (!isfinite (d))
      return 0;
    *score = d;
    return 1;
  }
  size_t
  mulib_embeddingsdb_knn (const mulib_embeddingsdb_t *db, const double *q, size_t n,
                          mulib_embeddings_metric_t metric, size_t k,
                          mulib_embeddings_hit_t *out)
  {
    if (!db || n != db->dimension || !mulib_embeddingsdb_vector_ (q, n)
        || (unsigned)metric > MULIB_EMBEDDINGS_DOT || (!out && k))
      return SIZE_MAX;
    size_t found = 0, limit = k < db->count ? k : db->count;
    mulib_embeddings_hit_t *tmp = NULL;
    if (limit > SIZE_MAX / sizeof (*tmp))
      return SIZE_MAX;
    if (limit)
      {
        tmp = (mulib_embeddings_hit_t *)malloc (limit * sizeof (*tmp));
        if (!tmp)
          return SIZE_MAX;
      }
    for (size_t i = 0; i < db->count; i++)
      {
        double score;
        if (!mulib_embeddingsdb_metric_ (q, db->entries[i].vector, n, metric, &score))
          {
            free (tmp);
            return SIZE_MAX;
          }
        size_t pos = 0;
        while (pos < found
               && (tmp[pos].distance < score
                   || (tmp[pos].distance == score && tmp[pos].id < db->entries[i].id)))
          pos++;
        if (pos == limit)
          continue;
        if (found < limit)
          found++;
        memmove (tmp + pos + 1, tmp + pos, (found - pos - 1) * sizeof (*tmp));
        tmp[pos] = (mulib_embeddings_hit_t){ db->entries[i].id, score };
      }
    if (found)
      memcpy (out, tmp, found * sizeof (*out));
    free (tmp);
    return found;
  }
  void
  mulib_embeddingsdb_free (mulib_embeddingsdb_t *db)
  {
    if (!db)
      return;
    for (size_t i = 0; i < db->count; i++)
      free (db->entries[i].vector);
    free (db->entries);
    memset (db, 0, sizeof (*db));
  }
#ifdef __cplusplus
}
#endif
#endif
