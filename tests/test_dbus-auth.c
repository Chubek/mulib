/* Generated, standalone MuLIB add-on symbol and API test: dbus-auth */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#include "mulib-dbus-auth.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_auth_external;
    assert (p);
  }
  puts ("PASS dbus-auth");
  return 0;
}
