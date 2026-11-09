#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_PROPERTIES
#define MULIB_IMPL_NM_CLIENT
#include "mulib-nm-client.h"
#include <stdio.h>
int
main (void)
{
  mulib_dbus_client_t c = MULIB_DBUS_CLIENT_INIT;
  int r = mulib_dbus_client_open (&c, NULL);
  uint32_t state = 0;
  if (!r)
    r = mulib_nm_client_state (&c, &state);
  printf ("NetworkManager: status=%d state=%u\n", r, state);
  mulib_dbus_client_close (&c);
  return 0;
}
