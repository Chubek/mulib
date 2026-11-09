/* Generated, standalone MuLIB add-on symbol and API test: uip-tcp */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_TCP
#include "mulib-uip-tcp.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_tcp_parse;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_tcp_pack;
    assert (p);
  }

  uint8_t f[64], a[] = { 10, 1, 2, 3 }, b[] = { 10, 1, 2, 4 };
  size_t n = 0;
  mulib_uip_tcp_segment_t u
      = { .source_port = 12, .dest_port = 13, .flags = 2, .window = 1024 };
  assert (!mulib_uip_tcp_pack (f, sizeof f, a, b, &u, &n));
  mulib_uip_tcp_segment_t v;
  assert (!mulib_uip_tcp_parse (a, b, f, n, &v));
  assert (v.flags == 2 && v.window == 1024);
  puts ("PASS uip-tcp");
  return 0;
}
