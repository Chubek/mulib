#define MULIB_IMPL_OCICONTAINERIPC
#define MULIB_IMPL_CONTAINER2DAEMONPROTO
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#include "mulib-ocicontaineripc.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  const void *data;
  size_t n, used;
  uint64_t channel;
  assert (mulib_ocicontaineripc_send (&b, 7, "hello", 5));
  assert (mulib_ocicontaineripc_receive (b.data, b.length, &channel, &data, &n,
                                         &used)
              == 1
          && channel == 7 && n == 5);
  mulib_buffer_free (&b);
  return 0;
}
