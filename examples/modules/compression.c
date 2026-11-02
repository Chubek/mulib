#define MULIB_IMPL_COMPRESSION
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_RLE
#include "mulib-compression.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t compressed = { 0 }, decoded = { 0 };
  assert (mulib_compression_encode (MULIB_COMPRESSION_RLE, "aaaaaaa", 7,
                                    &compressed));
  assert (mulib_compression_decode (MULIB_COMPRESSION_RLE, compressed.data,
                                    compressed.length, 7, &decoded));
  assert (decoded.length == 7 && !memcmp (decoded.data, "aaaaaaa", 7));
  mulib_buffer_free (&compressed);
  mulib_buffer_free (&decoded);
  return 0;
}
