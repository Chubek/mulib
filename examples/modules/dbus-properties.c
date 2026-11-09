#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_PROPERTIES
#include "mulib-dbus-properties.h"
#include <assert.h>
int
main (void)
{
  int pair[2];
  assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, pair));
  uint8_t frame[32] = { 'l', 2, 0, 1,   8, 0, 0, 0, 1, 0, 0,   0, 8, 0, 0,
                        0,   5, 1, 'u', 0, 1, 0, 0, 0, 1, 'u', 0, 0, 70 };
  assert (write (pair[1], frame, sizeof frame) == sizeof frame);
  mulib_dbus_client_t c = { pair[0], 0 };
  uint32_t value = 0;
  assert (!mulib_dbus_properties_u32 (&c, "org.example", "/org/example",
                                      "org.example", "State", &value));
  assert (value == 70);
  mulib_dbus_client_close (&c);
  close (pair[1]);
  return 0;
}
