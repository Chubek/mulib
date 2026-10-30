#define MULIB_IMPL_DBSUMMARY
#define MULIB_IMPL_MEMTABLE
#include "mulib-dbsummary.h"
#include <assert.h>
int
main (void)
{
  mulib_memtable_t t = { 0 };
  mulib_dbsummary_t s;
  assert (!mulib_memtable_put (&t, "k", 1, "v", 1, 1, 0));
  assert (mulib_dbsummary_compute (&t, &s) && s.live == 1
          && s.value_bytes == 1);
  mulib_memtable_free (&t);
  return 0;
}
