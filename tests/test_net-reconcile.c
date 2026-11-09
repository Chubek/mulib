/* Generated, standalone MuLIB add-on symbol and API test: net-reconcile */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NET_PROFILE
#define MULIB_IMPL_NET_RECONCILE
#define MULIB_IMPL_NET_STATE
#include "mulib-net-reconcile.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_reconcile_check;
    assert (p);
  }
  puts ("PASS net-reconcile");
  return 0;
}
