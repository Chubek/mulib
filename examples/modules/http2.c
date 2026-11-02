#define MULIB_IMPL_HTTP2
#define MULIB_IMPL_BUFFER
#include "mulib-http2.h"
#include <assert.h>
int
main (void)
{
  mulib_http2_frame_t data = { 0, 1, 1, (const unsigned char *)"hi", 2 },
                      decoded;
  mulib_buffer_t wire = { 0 };
  size_t used;
  assert (mulib_http2_frame_write (&wire, &data));
  assert (
      mulib_http2_frame_read (wire.data, wire.length, 16384, &decoded, &used)
      == 1);
  assert (decoded.stream == 1 && decoded.length == 2 && used == wire.length);
  mulib_buffer_free (&wire);
  return 0;
}
