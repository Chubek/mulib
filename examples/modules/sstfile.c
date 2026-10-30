#define MULIB_IMPL_SSTFILE
#define MULIB_IMPL_WAL
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#include "mulib-sstfile.h"
#include <assert.h>
int
main (void)
{
  mulib_memtable_t t = { 0 }, copy = { 0 };
  mulib_buffer_t b = { 0 };
  assert (!mulib_memtable_put (&t, "k", 1, "v", 1, 1, 0));
  assert (!mulib_sstfile_encode (&t, &b));
  assert (!mulib_sstfile_decode (b.data, b.length, &copy) && copy.count == 1);
  mulib_memtable_free (&copy);
  mulib_memtable_free (&t);
  mulib_buffer_free (&b);
  return 0;
}
