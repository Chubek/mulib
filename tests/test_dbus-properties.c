/* Generated, standalone MuLIB add-on symbol and API test: dbus-properties */
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
#include "mulib-dbus-properties.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_properties_u32;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_properties_string;
    assert (p);
  }
  puts ("PASS dbus-properties");
  return 0;
}
