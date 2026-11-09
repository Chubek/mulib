#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#define MULIB_IMPL_DBUS_CLIENT
#include "mulib-dbus-client.h"
#include <assert.h>
int
main (void)
{
  int pair[2];
  assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, pair));
  uint8_t frame[24]
      = { 'l', 2, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 5, 1, 'u', 0, 1 },
      reply[128];
  size_t n = 0;
  assert (write (pair[1], frame, sizeof frame) == sizeof frame);
  mulib_dbus_client_t c = { pair[0], 0 };
  assert (!mulib_dbus_client_call (&c, "org.example", "/org/example",
                                   "org.example", "Ping", NULL, NULL, reply,
                                   sizeof reply, &n));
  assert (n == 24);
  mulib_dbus_client_close (&c);
  close (pair[1]);
  return 0;
}
