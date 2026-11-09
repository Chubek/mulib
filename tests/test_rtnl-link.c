/* Generated, standalone MuLIB add-on symbol and API test: rtnl-link */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_LINK
#include "mulib-rtnl-link.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_link_index;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_link_set_up;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_link_dump;
    assert (p);
  }

  assert (mulib_rtnl_link_index ("lo") > 0);
  assert (mulib_rtnl_link_index (NULL) < 0);
  puts ("PASS rtnl-link");
  return 0;
}
