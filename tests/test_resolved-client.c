/* Generated, standalone MuLIB add-on symbol and API test: resolved-client */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_RESOLVED_CLIENT
#include "mulib-resolved-client.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_resolved_client_dns;
    assert (p);
  }
  puts ("PASS resolved-client");
  return 0;
}
