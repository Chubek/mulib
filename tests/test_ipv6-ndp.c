/* Generated, standalone MuLIB add-on symbol and API test: ipv6-ndp */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_IPV6_NDP
#include "mulib-ipv6-ndp.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ipv6_ndp_parse;
    assert (p);
  }

  uint8_t b[24] = { 135 };
  mulib_ipv6_ndp_t v;
  assert (!mulib_ipv6_ndp_parse (b, sizeof b, &v));
  assert (v.type == 135);
  puts ("PASS ipv6-ndp");
  return 0;
}
