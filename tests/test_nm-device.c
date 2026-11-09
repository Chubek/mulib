/* Generated, standalone MuLIB add-on symbol and API test: nm-device */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_PROPERTIES
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_NM_DEVICE
#include "mulib-nm-device.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_nm_device_interface;
    assert (p);
  }
  puts ("PASS nm-device");
  return 0;
}
