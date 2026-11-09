/* Generated, standalone MuLIB add-on symbol and API test: nfnetlink */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NFNETLINK
#include "mulib-nfnetlink.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_nfnetlink_open;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_nfnetlink_send;
    assert (p);
  }
  puts ("PASS nfnetlink");
  return 0;
}
