/* Generated, standalone MuLIB add-on symbol and API test: polkit-client */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_POLKIT_CLIENT
#include "mulib-polkit-client.h"
int
main (void)
{
  {
    void (*volatile p) (void)
        = (void (*) (void))&mulib_polkit_client_name_has_owner;
    assert (p);
  }
  puts ("PASS polkit-client");
  return 0;
}
