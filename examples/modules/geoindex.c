#define MULIB_IMPL_GEOINDEX
#include "mulib-geoindex.h"
#include <assert.h>
int
main (void)
{
  mulib_geoindex_t cities = { 0 };
  uint64_t ids[2];
  assert (mulib_geoindex_add (&cities, 1, 51.5074, -0.1278));
  assert (mulib_geoindex_add (&cities, 2, 48.8566, 2.3522));
  assert (mulib_geoindex_radius (&cities, 51.5074, -0.1278, 100, ids, 2) == 1
          && ids[0] == 1);
  mulib_geoindex_free (&cities);
  return 0;
}
