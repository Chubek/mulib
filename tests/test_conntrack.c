/* Generated, standalone MuLIB add-on symbol and API test: conntrack */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_CONNTRACK
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NFNETLINK
#include "mulib-conntrack.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_conntrack_dump;
    assert (p);
  }
  puts ("PASS conntrack");
  return 0;
}
