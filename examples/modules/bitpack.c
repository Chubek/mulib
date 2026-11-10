#define MULIB_IMPL_BITPACK
#include "mulib-bitpack.h"
#include <assert.h>
int
main (void)
{
  uint32_t values[] = { 1, 7, 3 }, decoded[3];
  unsigned char bytes[2];
  assert (mulib_bitpack_bytes (3, 3) == 2);
  assert (mulib_bitpack_encode (values, 3, 3, bytes, sizeof (bytes)));
  assert (mulib_bitpack_decode (bytes, sizeof (bytes), 3, 3, decoded));
  assert (decoded[0] == 1 && decoded[1] == 7 && decoded[2] == 3);
  return 0;
}
