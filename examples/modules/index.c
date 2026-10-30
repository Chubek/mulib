#define MULIB_IMPL_INDEX
#define MULIB_IMPL_BPTREE
#include "mulib-index.h"
#include <assert.h>
int
main (void)
{
  mulib_index_t t = { 0 };
  uint64_t row;
  assert (mulib_index_insert (&t, 42, 1));
  assert (!mulib_index_insert (&t, 42, 2));
  assert (mulib_index_find (&t, 42, &row) && row == 1);
  mulib_index_free (&t);
  return 0;
}
