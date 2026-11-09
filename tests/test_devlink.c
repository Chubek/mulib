/* Generated, standalone MuLIB add-on symbol and API test: devlink */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DEVLINK
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#include "mulib-devlink.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_devlink_family;
    assert (p);
  }
  puts ("PASS devlink");
  return 0;
}
