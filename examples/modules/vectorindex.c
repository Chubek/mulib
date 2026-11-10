#define MULIB_IMPL_VECTORINDEX
#include "mulib-vectorindex.h"
#include <assert.h>
int
main (void)
{
  mulib_vectorindex_t index = { 0 };
  index.dimension = 2;
  double vector[] = { 1, 2 }, query[] = { 1, 3 }, distance;
  uint64_t id;
  assert (mulib_vectorindex_insert (&index, 42, vector, 2));
  assert (mulib_vectorindex_nearest (&index, query, 2, &id, &distance) && id == 42
          && distance == 1);
  mulib_vectorindex_free (&index);
  return 0;
}
