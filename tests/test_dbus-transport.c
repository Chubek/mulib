/* Generated, standalone MuLIB add-on symbol and API test: dbus-transport */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#include "mulib-dbus-transport.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_transport_connect;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_transport_write;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_transport_read;
    assert (p);
  }

  assert (mulib_dbus_transport_connect ("/definitely/no/such/socket") < 0);
  puts ("PASS dbus-transport");
  return 0;
}
