#define MULIB_IMPL_DBQUERY
#define MULIB_IMPL_MEMTABLE
#include "mulib-dbquery.h"
#include <assert.h>
static int
visit (const mulib_memtable_record_t *r, void *ctx)
{
  (void)r;
  ++*(size_t *)ctx;
  return 1;
}
int
main (void)
{
  mulib_memtable_t t = { 0 };
  size_t count = 0;
  assert (!mulib_memtable_put (&t, "k", 1, "v", 1, 1, 0));
  assert (mulib_dbquery_scan (&t, NULL, 0, NULL, 0, 10, visit, &count) == 1
          && count == 1);
  mulib_memtable_free (&t);
  return 0;
}
