#define MULIB_IMPL_ZONEINDEX
#include "mulib-zoneindex.h"
#include <assert.h>
int
main (void)
{
  mulib_zoneindex_zone_t zones[] = { { 0, 9 }, { 10, 19 }, { 20, 29 } };
  size_t blocks[3];
  assert (mulib_zoneindex_filter (zones, 3, 15, 22, blocks, 3) == 2 && blocks[0] == 1
          && blocks[1] == 2);
  return 0;
}
