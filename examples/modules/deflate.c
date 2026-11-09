#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_DEFLATE
#include "mulib-deflate.h"
#include <assert.h>
int
main (void)
{
  const char text[] = "hello hello hello hello";
  mulib_buffer_t encoded = { 0 }, decoded = { 0 };
  assert (mulib_deflate_encode (text, sizeof (text) - 1, &encoded) == 0);
  assert (mulib_deflate_decode (encoded.data, encoded.length, 100, &decoded)
          == 0);
  assert (decoded.length == sizeof (text) - 1
          && !memcmp (decoded.data, text, decoded.length));
  mulib_buffer_free (&encoded);
  mulib_buffer_free (&decoded);
  return 0;
}
