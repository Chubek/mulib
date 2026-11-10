#define MULIB_IMPL_RANGEINDEX
#include "mulib-rangeindex.h"
#include <assert.h>
static int
visit (uint64_t id, void *ctx)
{
  *(uint64_t *)ctx = id;
  return 1;
}
int
main (void)
{
  mulib_rangeindex_t ranges = { 0 };
  uint64_t id = 0;
  assert (mulib_rangeindex_insert (&ranges, 10, 20, 42));
  assert (mulib_rangeindex_overlap (&ranges, 20, 20, visit, &id) == 1 && id == 42);
  mulib_rangeindex_free (&ranges);
  return 0;
}
