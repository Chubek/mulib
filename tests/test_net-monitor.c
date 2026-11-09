/* Generated, standalone MuLIB add-on symbol and API test: net-monitor */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NET_MONITOR
#define MULIB_IMPL_NETLINK
#include "mulib-net-monitor.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_monitor_open;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_monitor_read;
    assert (p);
  }
  puts ("PASS net-monitor");
  return 0;
}
