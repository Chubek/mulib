/* Generated, standalone MuLIB add-on symbol and API test: packet-socket */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_PACKET_SOCKET
#include "mulib-packet-socket.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_packet_socket_open;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_packet_socket_receive;
    assert (p);
  }
  puts ("PASS packet-socket");
  return 0;
}
