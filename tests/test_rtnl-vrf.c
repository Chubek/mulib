/* Generated, standalone MuLIB add-on symbol and API test: rtnl-vrf */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_LINK
#define MULIB_IMPL_RTNL_VRF
#include "mulib-rtnl-vrf.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_vrf_dump;
    assert (p);
  }
  puts ("PASS rtnl-vrf");
  return 0;
}
