#define MULIB_IMPL_OBJECTDB
#define MULIB_IMPL_MEMTABLE
#include "mulib-objectdb.h"
#include <assert.h>
int
main (void)
{
  mulib_objectdb_t db = { 0 };
  const void *bytes;
  size_t n;
  assert (!mulib_objectdb_put (&db, "id", 2, 7, "opaque", 6, 1));
  assert (!mulib_objectdb_get (&db, "id", 2, 7, &bytes, &n) && n == 6);
  assert (!memcmp (bytes, "opaque", 6));
  mulib_objectdb_free (&db);
  return 0;
}
