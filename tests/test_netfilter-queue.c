/* Generated, standalone MuLIB add-on symbol and API test: netfilter-queue */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETFILTER_QUEUE
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NFNETLINK
#include "mulib-netfilter-queue.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netfilter_queue_bind;
    assert (p);
  }
  puts ("PASS netfilter-queue");
  return 0;
}
