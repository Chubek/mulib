/* Offline authentication peer, not a system daemon. */
#define MULIB_IMPL_DBUS_AUTH
#include "mulib-dbus-auth.h"
#include <assert.h>
int
main (void)
{
  int pair[2];
  assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, pair));
  const char ok[] = "OK 0123456789abcdef0123456789abcdef\r\n";
  assert (write (pair[1], ok, sizeof ok - 1) == sizeof ok - 1);
  assert (!mulib_dbus_auth_external (pair[0]));
  close (pair[0]);
  close (pair[1]);
  return 0;
}
