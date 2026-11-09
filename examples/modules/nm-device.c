#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_PROPERTIES
#define MULIB_IMPL_NM_DEVICE
#include "mulib-nm-device.h"
#include <assert.h>
/* The object path must first be obtained from GetDevices. */
int
main (void)
{
  mulib_dbus_client_t c = MULIB_DBUS_CLIENT_INIT;
  char name[32];
  assert (
      mulib_nm_device_interface (
          &c, "/org/freedesktop/NetworkManager/Devices/1", name, sizeof name)
      == -EINVAL);
  return 0;
}
