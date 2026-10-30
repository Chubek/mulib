#define MULIB_IMPL_BLOCKIDX
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BYCUTILS
#include "mulib-blockidx.h"
#include <assert.h>
int
main (void)
{
  mulib_blockidx_t t = { 0 };
  uint64_t offset;
  assert (!mulib_blockidx_add (&t, "a", 1, 128));
  assert (!mulib_blockidx_find (&t, "b", 1, &offset) && offset == 128);
  mulib_blockidx_free (&t);
  return 0;
}
