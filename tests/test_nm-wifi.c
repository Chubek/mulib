/* Generated, standalone MuLIB add-on symbol and API test: nm-wifi */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_NM_WIFI
#include "mulib-nm-wifi.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_nm_wifi_access_points;
    assert (p);
  }
  puts ("PASS nm-wifi");
  return 0;
}
