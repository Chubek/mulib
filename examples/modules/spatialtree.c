#define MULIB_IMPL_SPATIALTREE
#include "mulib-spatialtree.h"
#include <assert.h>
static int
visit (const mulib_spatialtree_point_t *p, void *ctx)
{
  *(uint64_t *)ctx = p->id;
  return 1;
}
int
main (void)
{
  mulib_spatialtree_t t;
  mulib_spatialtree_point_t points[] = { { 1, 2, 7 }, { 9, 9, 8 } };
  uint64_t id = 0;
  assert (mulib_spatialtree_build (points, 2, &t));
  assert (mulib_spatialtree_query (&t, 0, 0, 3, 3, visit, &id) == 1
          && id == 7);
  mulib_spatialtree_free (&t);
  return 0;
}
