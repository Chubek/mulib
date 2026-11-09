/* Generated, standalone MuLIB add-on symbol and API test: uip-ethernet */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_ETHERNET
#include "mulib-uip-ethernet.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_ethernet_parse;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_ethernet_pack;
    assert (p);
  }

  uint8_t d[6] = { 1 }, s[6] = { 2 }, f[40];
  size_t n = 0;
  mulib_uip_ethernet_frame_t v;
  assert (!mulib_uip_ethernet_pack (f, sizeof f, d, s, 0x0800, "x", 1, &n));
  assert (!mulib_uip_ethernet_parse (f, n, &v));
  assert (v.ethertype == 0x0800 && v.length == 1);
  puts ("PASS uip-ethernet");
  return 0;
}
