#define MULIB_IMPL_UIP_IPV6
#include "mulib-uip-ipv6.h"
#include <assert.h>
int
main (void)
{
  uint8_t src[16] = { 0x20, 1 }, dst[16] = { 0x20, 2 }, bytes[64];
  size_t n = 0;
  assert (!mulib_uip_ipv6_pack (bytes, sizeof bytes, src, dst, 17, 64, "data",
                                4, &n));
  mulib_uip_ipv6_packet_t view;
  assert (!mulib_uip_ipv6_parse (bytes, n, &view));
  assert (view.payload_length == 4);
  return 0;
}
