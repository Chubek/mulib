#define MULIB_IMPL_CONTAINER2DAEMONPROTO
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#include "mulib-container2daemonproto.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  mulib_container2daemonproto_frame_t f;
  size_t used;
  assert (mulib_container2daemonproto_encode (&b, 4, 42, "demo", 4));
  assert (mulib_container2daemonproto_decode (b.data, b.length, &f, &used) == 1
          && f.request_id == 42);
  mulib_buffer_free (&b);
  return 0;
}
