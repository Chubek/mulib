/* Generated, standalone MuLIB add-on symbol and API test: dbus-client */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_TRANSPORT
#include "mulib-dbus-client.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_client_open;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_client_close;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dbus_client_call;
    assert (p);
  }
  {
    void (*volatile p) (void)
        = (void (*) (void))&mulib_dbus_client_body_offset;
    assert (p);
  }

  uint8_t b[16] = { 0 };
  assert (mulib_dbus_client_body_offset (b, sizeof b) == 0);
  puts ("PASS dbus-client");
  return 0;
}
