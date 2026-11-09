#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_IPV4
#include "mulib-uip-ipv4.h"
#include <assert.h>
int
main (void)
{
  uint8_t src[4] = { 10, 0, 0, 1 }, dst[4] = { 10, 0, 0, 2 }, bytes[64];
  size_t n = 0;
  assert (!mulib_uip_ipv4_pack (bytes, sizeof bytes, src, dst, 17, 64, 1,
                                "data", 4, &n));
  mulib_uip_ipv4_packet_t view;
  assert (!mulib_uip_ipv4_parse (bytes, n, &view));
  assert (view.payload_length == 4);
  return 0;
}
