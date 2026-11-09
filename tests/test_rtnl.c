/* Generated, standalone MuLIB add-on symbol and API test: rtnl */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#include "mulib-rtnl.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_request;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_dump;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_link_kind;
    assert (p);
  }
  puts ("PASS rtnl");
  return 0;
}
