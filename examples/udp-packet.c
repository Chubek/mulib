#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_UDP
#include "mulib-uip-udp.h"
#include <stdio.h>
int
main (void)
{
  uint8_t src[4] = { 10, 0, 0, 1 }, dst[4] = { 10, 0, 0, 2 }, buf[128];
  size_t used = 0;
  int r = mulib_uip_udp_pack (buf, sizeof buf, src, dst, 10000, 53, "hello", 5,
                              &used);
  if (r)
    return 1;
  mulib_uip_udp_datagram_t parsed;
  if (mulib_uip_udp_parse (src, dst, buf, used, &parsed))
    return 1;
  printf ("UDP datagram: %zu bytes, %u->%u, payload %zu bytes\n", used,
          parsed.src_port, parsed.dst_port, parsed.length);
  return 0;
}
