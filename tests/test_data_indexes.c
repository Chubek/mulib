#define MULIB_IMPL_RANGEINDEX
#define MULIB_IMPL_GEOINDEX
#define MULIB_IMPL_VECTORINDEX
#define MULIB_IMPL_EMBEDDINGSDB
#include "mulib-rangeindex.h"
#include "mulib-geoindex.h"
#include "mulib-vectorindex.h"
#include "mulib-embeddingsdb.h"
#include <assert.h>
#include <float.h>

static int
visit (uint64_t id, void *ctx)
{
  *(uint64_t *)ctx = id;
  return 0;
}
int
main (void)
{
  mulib_rangeindex_t r = { 0 };
  uint64_t id = 0;
  assert (mulib_rangeindex_insert (&r, 10, 20, 1)
          && mulib_rangeindex_insert (&r, -10, 10, 2));
  assert (!mulib_rangeindex_insert (&r, 2, 1, 3));
  assert (mulib_rangeindex_overlap (&r, 10, 10, visit, &id) == 1 && id == 2);
  assert (mulib_rangeindex_overlap (&r, 21, 22, visit, &id) == 0);
  mulib_rangeindex_free (&r);
  mulib_geoindex_t geo = { 0 };
  uint64_t ids[2] = { 99 };
  assert (mulib_geoindex_add (&geo, 1, 0, 0) && mulib_geoindex_add (&geo, 2, 0, 1));
  assert (!mulib_geoindex_add (&geo, 3, 91, 0));
  assert (fabs (mulib_geoindex_distance_km (0, 0, 0, 1) - 111.19492664455873) < 1e-9);
  assert (mulib_geoindex_distance_km (0, 180, 0, -180) < 1e-9);
  assert (mulib_geoindex_radius (&geo, 0, 0, 112, ids, 1) == SIZE_MAX && ids[0] == 99);
  assert (mulib_geoindex_radius (&geo, 0, 0, 112, ids, 2) == 2);
  assert (mulib_geoindex_add (&geo, 1, 0, 2) && geo.count == 2);
  mulib_geoindex_free (&geo);
  mulib_vectorindex_t vi = { 0 };
  vi.dimension = 2;
  mulib_embeddingsdb_t db = { 2, 0, NULL };
  double a[] = { 1, 0 }, b[] = { -1, 0 }, q[] = { 0, 0 }, dist;
  assert (mulib_vectorindex_insert (&vi, 9, a, 2)
          && mulib_vectorindex_insert (&vi, 3, b, 2));
  assert (mulib_embeddingsdb_put (&db, 9, a, 2)
          && mulib_embeddingsdb_put (&db, 3, b, 2));
  assert (mulib_vectorindex_nearest (&vi, q, 2, &id, &dist) && id == 3 && dist == 1);
  assert (mulib_embeddingsdb_nearest (&db, q, 2, &id, &dist) && id == 3 && dist == 1);
  mulib_embeddings_hit_t hits[2] = { { 99, 99 } };
  assert (mulib_embeddingsdb_knn (&db, q, 2, MULIB_EMBEDDINGS_EUCLIDEAN, 2, hits) == 2
          && hits[0].id == 3);
  hits[0].id = 99;
  assert (mulib_embeddingsdb_knn (&db, q, 2, MULIB_EMBEDDINGS_COSINE, 2, hits)
              == SIZE_MAX
          && hits[0].id == 99);
  assert (mulib_embeddingsdb_knn (&db, a, 2, MULIB_EMBEDDINGS_COSINE, 2, hits) == 2
          && hits[0].id == 9 && hits[0].distance == 0 && hits[1].distance == 2);
  assert (mulib_embeddingsdb_knn (&db, a, 2, MULIB_EMBEDDINGS_DOT, 2, hits) == 2
          && hits[0].distance == -1);
  assert (mulib_embeddingsdb_knn (&db, a, 2, (mulib_embeddings_metric_t)-1, 2, hits)
          == SIZE_MAX);
  a[0] = DBL_MAX;
  a[1] = DBL_MAX;
  b[0] = DBL_MIN;
  b[1] = DBL_MIN;
  assert (mulib_embeddingsdb_put (&db, 9, a, 2)
          && mulib_embeddingsdb_put (&db, 3, b, 2));
  assert (mulib_embeddingsdb_knn (&db, a, 2, MULIB_EMBEDDINGS_COSINE, 2, hits) == 2);
  assert (fabs (hits[0].distance) < 1e-15 && fabs (hits[1].distance) < 1e-15);
  assert (mulib_vectorindex_insert (&vi, 9, vi.entries[0].values, 2) && vi.count == 2);
  assert (mulib_embeddingsdb_put (&db, 9, db.entries[0].vector, 2) && db.count == 2);
  a[0] = NAN;
  assert (!mulib_embeddingsdb_put (&db, 9, a, 2)
          && !mulib_vectorindex_insert (&vi, 9, a, 2));
  assert (mulib_embeddingsdb_delete (&db, 9) && !mulib_embeddingsdb_delete (&db, 9));
  mulib_embeddingsdb_free (&db);
  mulib_vectorindex_free (&vi);
  return 0;
}
