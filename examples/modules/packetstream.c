#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_PACKETSTREAM
#include "mulib-packetstream.h"
#include <assert.h>
#include <string.h>
static int
packet (void *ctx, const void *p, size_t n)
{
  ++*(int *)ctx;
  assert (n == 3 && !memcmp (p, "abc", 3));
  return 0;
}
int
main (void)
{
  unsigned char wire[16], buffer[16];
  size_t n, used;
  int count = 0;
  mulib_packetstream_t s;
  assert (mulib_packetstream_init (&s, buffer, sizeof buffer));
  assert (mulib_packetstream_encode ("abc", 3, wire, sizeof wire, &n));
  for (size_t i = 0; i < n; ++i)
    assert (mulib_packetstream_feed (&s, wire + i, 1, &used, packet, &count)
                == 0
            && used == 1);
  assert (count == 1 && mulib_packetstream_finished (&s));
  return 0;
}
