#define MULIB_IMPL_DBINDEX
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BYCUTILS
#include "mulib-dbindex.h"
#include <assert.h>
int
main (void)
{
  mulib_dbindex_t t = { 0 };
  uint64_t row;
  assert (!mulib_dbindex_set (&t, "name", 4, 42));
  assert (!mulib_dbindex_get (&t, "name", 4, &row) && row == 42);
  mulib_dbindex_free (&t);
  return 0;
}
