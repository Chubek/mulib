#define MULIB_IMPL_COWBPTREE
#define MULIB_IMPL_BPTREE
#include "mulib-cowb+tree.h"
#include <assert.h>
int
main (void)
{
  mulib_cowbptree_t a = { 0 }, snapshot = { 0 };
  uint64_t v;
  assert (mulib_cowbptree_set (&a, 1, 10));
  assert (mulib_cowbptree_snapshot (&a, &snapshot));
  assert (mulib_cowbptree_set (&a, 1, 20));
  assert (mulib_cowbptree_get (&snapshot, 1, &v) && v == 10);
  mulib_cowbptree_free (&a);
  mulib_cowbptree_free (&snapshot);
  return 0;
}
