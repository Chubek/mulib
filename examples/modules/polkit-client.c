#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_POLKIT_CLIENT
#include "mulib-polkit-client.h"
#include <stdio.h>
int
main (void)
{
  mulib_dbus_client_t c = MULIB_DBUS_CLIENT_INIT;
  int r = mulib_dbus_client_open (&c, NULL), present = 0;
  if (!r)
    r = mulib_polkit_client_name_has_owner (&c, &present);
  printf ("PolicyKit: status=%d present=%d\n", r, present);
  mulib_dbus_client_close (&c);
  return 0;
}
