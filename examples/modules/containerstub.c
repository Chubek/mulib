#define MULIB_IMPL_CONTAINERSTUB
#define MULIB_IMPL_OCICONTAINER
#define MULIB_IMPL_CONTAINER2DAEMONPROTO
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#include "mulib-containerstub.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  assert (mulib_containerstub_request (&b, MULIB_CONTAINER2DAEMONPROTO_STATUS,
                                       1, "demo"));
  mulib_buffer_free (&b);
  return 0;
}
