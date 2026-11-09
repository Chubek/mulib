#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_NETWORKD_CLIENT
#include "mulib-networkd-client.h"
#include <stdio.h>
int
main (void)
{
  mulib_dbus_client_t c = MULIB_DBUS_CLIENT_INIT;
  int r = mulib_dbus_client_open (&c, NULL);
  uint8_t reply[16384];
  size_t n = 0;
  if (!r)
    r = mulib_networkd_client_links (&c, reply, sizeof reply, &n);
  printf ("networkd: status=%d frame-bytes=%zu\n", r, n);
  mulib_dbus_client_close (&c);
  return 0;
}
