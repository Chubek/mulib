#define MULIB_IMPL_EMBEDDINGSDB
#include "mulib-embeddingsdb.h"
#include <assert.h>
int
main (void)
{
  mulib_embeddingsdb_t db = { 2, 0, NULL };
  const double vector[] = { 1, 2 }, query[] = { 1.1, 2.1 };
  uint64_t id;
  double distance;
  assert (mulib_embeddingsdb_put (&db, 42, vector, 2));
  assert (mulib_embeddingsdb_nearest (&db, query, 2, &id, &distance));
  assert (id == 42 && distance < 0.15);
  mulib_embeddings_hit_t hits[1];
  assert (mulib_embeddingsdb_knn (&db, query, 2, MULIB_EMBEDDINGS_COSINE, 1, hits)
          == 1);
  assert (hits[0].id == 42 && hits[0].distance < 0.01);
  mulib_embeddingsdb_free (&db);
  return 0;
}
