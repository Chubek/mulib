#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#include "mulib-binfilewriter.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  assert (mulib_binfilewriter_uint (&b, 2, 0x1234));
  assert (mulib_binfilewriter_align (&b, 8) && b.length == 8
          && b.data[0] == 0x34);
  mulib_buffer_free (&b);
  return 0;
}
