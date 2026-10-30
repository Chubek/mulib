#define MULIB_IMPL_BINFILEREADER
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_BYCUTILS
#include "mulib-binfilereader.h"
#include <assert.h>
int
main (void)
{
  unsigned char data[] = { 0x34, 0x12 };
  mulib_binfilereader_t r;
  uint64_t v;
  assert (mulib_binfilereader_init (&r, data, 2));
  assert (mulib_binfilereader_uint (&r, 2, &v) && v == 0x1234);
  assert (!mulib_binfilereader_uint (&r, 1, &v));
  return 0;
}
