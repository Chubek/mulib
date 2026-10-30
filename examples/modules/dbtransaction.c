#define MULIB_IMPL_DBTRANSACTION
#define MULIB_IMPL_MEMTABLE
#include "mulib-dbtransaction.h"
#include <assert.h>
int
main (void)
{
  mulib_memtable_t t = { 0 };
  mulib_dbtransaction_t tx = { 0 };
  assert (!mulib_dbtransaction_begin (&tx, &t));
  assert (!mulib_memtable_put (&tx.working, "k", 1, "v", 1, 1, 0));
  assert (!mulib_dbtransaction_commit (&tx) && t.count == 1);
  mulib_memtable_free (&t);
  return 0;
}
