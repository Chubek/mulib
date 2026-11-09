/* Generated, standalone MuLIB add-on symbol and API test: netlink-async */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ASYNC
#include "mulib-netlink-async.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_async_wait;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_async_recv;
    assert (p);
  }
  puts ("PASS netlink-async");
  return 0;
}
