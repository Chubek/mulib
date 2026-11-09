/* Generated, standalone MuLIB add-on symbol and API test: wifi-ap */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_NL80211
#define MULIB_IMPL_WIFI_AP
#include "mulib-wifi-ap.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_wifi_ap_query;
    assert (p);
  }
  puts ("PASS wifi-ap");
  return 0;
}
