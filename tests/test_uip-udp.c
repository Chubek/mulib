/* Generated, standalone MuLIB add-on symbol and API test: uip-udp */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_UDP
#include "mulib-uip-udp.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_udp_parse;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_udp_pack;
    assert (p);
  }

  uint8_t f[64], a[] = { 10, 1, 2, 3 }, b[] = { 10, 1, 2, 4 };
  size_t n = 0;
  mulib_uip_udp_datagram_t u;
  assert (!mulib_uip_udp_pack (f, sizeof f, a, b, 22, 33, "abc", 3, &n));
  assert (!mulib_uip_udp_parse (a, b, f, n, &u));
  assert (u.length == 3 && u.dst_port == 33);
  puts ("PASS uip-udp");
  return 0;
}
