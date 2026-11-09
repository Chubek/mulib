/* Generated, standalone MuLIB add-on symbol and API test: ipv6-ra */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_IPV6_RA
#include "mulib-ipv6-ra.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ipv6_ra_parse;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ipv6_ra_prefix;
    assert (p);
  }

  uint8_t b[48] = { 134 };
  b[16] = 3;
  b[17] = 4;
  b[18] = 64;
  b[32] = 0x20;
  uint8_t p[16] = { 0 };
  unsigned mask = 0;
  assert (!mulib_ipv6_ra_prefix (b, sizeof b, p, &mask));
  assert (mask == 64 && p[0] == 0x20);
  puts ("PASS ipv6-ra");
  return 0;
}
