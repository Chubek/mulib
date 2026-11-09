/* Generated, standalone MuLIB add-on symbol and API test: wifi-monitor */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_WIFI_MONITOR
#include "mulib-wifi-monitor.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_wifi_monitor_open;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_wifi_monitor_next;
    assert (p);
  }
  puts ("PASS wifi-monitor");
  return 0;
}
