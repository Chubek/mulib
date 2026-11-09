#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_NM_WIFI
#include "mulib-nm-wifi.h"
#include <assert.h>
int
main (void)
{
  mulib_dbus_client_t c = MULIB_DBUS_CLIENT_INIT;
  uint8_t reply[128];
  size_t n = 0;
  assert (mulib_nm_wifi_access_points (
              &c, "/org/freedesktop/NetworkManager/Devices/1", reply,
              sizeof reply, &n)
          == -EINVAL);
  return 0;
}
