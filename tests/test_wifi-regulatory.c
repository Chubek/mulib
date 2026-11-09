/* Generated, standalone MuLIB add-on symbol and API test: wifi-regulatory */
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
#define MULIB_IMPL_WIFI_REGULATORY
#include "mulib-wifi-regulatory.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_wifi_regulatory_query;
    assert (p);
  }
  puts ("PASS wifi-regulatory");
  return 0;
}
