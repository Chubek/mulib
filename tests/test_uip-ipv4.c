/* Generated, standalone MuLIB add-on symbol and API test: uip-ipv4 */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_IPV4
#include "mulib-uip-ipv4.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_ipv4_parse;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_ipv4_pack;
    assert (p);
  }

  uint8_t f[64], a[] = { 10, 0, 0, 1 }, b[] = { 10, 0, 0, 2 };
  size_t n = 0;
  mulib_uip_ipv4_packet_t p;
  assert (!mulib_uip_ipv4_pack (f, sizeof f, a, b, 17, 64, 1, "hi", 2, &n));
  assert (!mulib_uip_ipv4_parse (f, n, &p));
  assert (p.protocol == 17 && p.payload_length == 2);
  puts ("PASS uip-ipv4");
  return 0;
}
