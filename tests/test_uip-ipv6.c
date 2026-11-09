/* Generated, standalone MuLIB add-on symbol and API test: uip-ipv6 */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_IPV6
#include "mulib-uip-ipv6.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_ipv6_parse;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_ipv6_pack;
    assert (p);
  }

  uint8_t f[80], a[16] = { 1 }, b[16] = { 2 };
  size_t n = 0;
  mulib_uip_ipv6_packet_t p;
  assert (!mulib_uip_ipv6_pack (f, sizeof f, a, b, 17, 64, "hi", 2, &n));
  assert (!mulib_uip_ipv6_parse (f, n, &p));
  assert (p.payload_length == 2);
  puts ("PASS uip-ipv6");
  return 0;
}
