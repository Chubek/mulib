/* Avoid CAP_NET_RAW: demonstrate argument validation, not raw capture. */
#define MULIB_IMPL_PACKET_SOCKET
#include "mulib-packet-socket.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_packet_socket_open (0, 0x0800) == -EINVAL);
  return 0;
}
