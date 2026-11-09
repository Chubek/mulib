/* Generated, standalone MuLIB add-on symbol and API test: nftables */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NFNETLINK
#define MULIB_IMPL_NFTABLES
#include "mulib-nftables.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_nftables_tables;
    assert (p);
  }
  puts ("PASS nftables");
  return 0;
}
