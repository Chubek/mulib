/* Generated, standalone MuLIB add-on symbol and API test: inet-diag */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_INET_DIAG
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_SOCK_DIAG
#include "mulib-inet-diag.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_inet_diag_decode;
    assert (p);
  }

  mulib_inet_diag_socket_t s;
  assert (mulib_inet_diag_decode (NULL, 0, &s) < 0);
  puts ("PASS inet-diag");
  return 0;
}
