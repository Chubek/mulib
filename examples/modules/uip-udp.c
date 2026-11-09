#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_UDP
#include "mulib-uip-udp.h"
#include <assert.h>
int
main (void)
{
  uint8_t src[4] = { 10, 0, 0, 1 }, dst[4] = { 10, 0, 0, 2 }, bytes[64];
  size_t n = 0;
  assert (!mulib_uip_udp_pack (bytes, sizeof bytes, src, dst, 1000, 2000,
                               "data", 4, &n));
  mulib_uip_udp_datagram_t view;
  assert (!mulib_uip_udp_parse (src, dst, bytes, n, &view));
  assert (view.dst_port == 2000);
  return 0;
}
