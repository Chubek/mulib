#define MULIB_IMPL_HTTP3
#define MULIB_IMPL_QUIC
#define MULIB_IMPL_BUFFER
#include "mulib-http3.h"
#include <assert.h>
int
main (void)
{
  mulib_http3_frame_t data = { 0, (const unsigned char *)"hi", 2 }, decoded;
  mulib_buffer_t wire = { 0 };
  size_t used;
  assert (mulib_http3_frame_write (&wire, &data));
  assert (mulib_http3_frame_read (wire.data, wire.length, 100, &decoded, &used)
          == 1);
  assert (decoded.type == 0 && decoded.length == 2 && used == wire.length);
  mulib_buffer_free (&wire);
  return 0;
}
