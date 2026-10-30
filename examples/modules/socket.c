#define MULIB_IMPL_SOCKET
#include "mulib-socket.h"
#include <assert.h>
int
main (void)
{
  mulib_socket_t a = MULIB_SOCKET_INIT, b = MULIB_SOCKET_INIT;
  int rc = mulib_socket_pair (&a, &b);
  if (rc == MULIB_SOCKET_ENOTSUP)
    return 0;
  assert (!rc);
  assert (!mulib_socket_close (&a) && !mulib_socket_close (&b));
  return 0;
}
