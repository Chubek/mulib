#define MULIB_IMPL_LSMTREE
#define MULIB_IMPL_SSTFILE
#define MULIB_IMPL_WAL
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#include "mulib-lsmtree.h"
#include <assert.h>
int
main (void)
{
  mulib_lsmtree_t t = { 0 };
  mulib_buffer_t value = { 0 };
  assert (!mulib_lsmtree_put (&t, "k", 1, "v", 1, 0));
  assert (!mulib_lsmtree_flush (&t));
  assert (!mulib_lsmtree_get (&t, "k", 1, &value) && value.length == 1);
  mulib_buffer_free (&value);
  mulib_lsmtree_free (&t);
  return 0;
}
