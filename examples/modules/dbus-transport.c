#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_TRANSPORT
#include "mulib-dbus-transport.h"
#include <assert.h>
int
main (void)
{
  int pair[2];
  assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, pair));
  uint8_t frame[16] = { 'l', 2, 0, 1, 0, 0, 0, 0, 1 }, reply[16];
  size_t n = 0;
  assert (!mulib_dbus_transport_write (pair[0], frame, sizeof frame));
  assert (!mulib_dbus_transport_read (pair[1], reply, sizeof reply, &n));
  assert (n == 16);
  close (pair[0]);
  close (pair[1]);
  return 0;
}
