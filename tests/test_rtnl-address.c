/* Generated, standalone MuLIB add-on symbol and API test: rtnl-address */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_ADDRESS
#include "mulib-rtnl-address.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_address_dump;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_address_change;
    assert (p);
  }

  assert (mulib_rtnl_address_dump (2, NULL, NULL) < 0);
  puts ("PASS rtnl-address");
  return 0;
}
