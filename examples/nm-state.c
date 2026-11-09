#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_PROPERTIES
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_NM_CLIENT
#include "mulib-nm-client.h"
#include <stdio.h>
int
main (void)
{
  mulib_dbus_client_t c = MULIB_DBUS_CLIENT_INIT;
  int r = mulib_dbus_client_open (&c, NULL);
  if (r)
    {
      printf ("NetworkManager system D-Bus unavailable (%d)\n", r);
      return 0;
    }
  uint32_t state = 0;
  r = mulib_nm_client_state (&c, &state);
  printf ("NetworkManager state: %u (err=%d)\n", state, r);
  mulib_dbus_client_close (&c);
  return 0;
}
