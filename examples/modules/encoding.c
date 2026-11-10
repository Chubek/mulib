#define MULIB_IMPL_ENCODING
#include "mulib-encoding.h"
#include <assert.h>
int
main (void)
{
  unsigned char bytes[10];
  uint64_t value;
  size_t n = mulib_encoding_uvarint (mulib_encoding_zigzag (-123), bytes);
  assert (mulib_encoding_read_uvarint (bytes, n, &value) == n);
  assert (mulib_encoding_unzigzag (value) == -123);
  return 0;
}
