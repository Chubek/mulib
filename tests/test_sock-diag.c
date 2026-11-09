/* Generated, standalone MuLIB add-on symbol and API test: sock-diag */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_SOCK_DIAG
#include "mulib-sock-diag.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_sock_diag_dump;
    assert (p);
  }
  puts ("PASS sock-diag");
  return 0;
}
