#define MULIB_IMPL_FLATBUFFERS
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_BYCUTILS
#include "mulib-flatbuffers.h"
#include <assert.h>
int
main (void)
{
  /* Root at 12, vtable at 4, uint32 field at 16. */
  unsigned char bytes[20]
      = { 12, 0, 0, 0, 6, 0, 8, 0, 4, 0, 0, 0, 8, 0, 0, 0, 42, 0, 0, 0 };
  mulib_flatbuffers_table_t t;
  uint64_t value;
  assert (mulib_flatbuffers_root (bytes, 20, &t));
  assert (mulib_flatbuffers_uint (&t, 0, 4, &value) && value == 42);
  return 0;
}
