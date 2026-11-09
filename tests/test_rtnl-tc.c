/* Generated, standalone MuLIB add-on symbol and API test: rtnl-tc */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_TC
#include "mulib-rtnl-tc.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_tc_dump;
    assert (p);
  }
  puts ("PASS rtnl-tc");
  return 0;
}
