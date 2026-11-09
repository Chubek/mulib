/* Generated, standalone MuLIB add-on symbol and API test: dns-manager */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DNS_MANAGER
#include "mulib-dns-manager.h"
int
main (void)
{
  {
    void (*volatile p) (void)
        = (void (*) (void))&mulib_dns_manager_first_nameserver;
    assert (p);
  }
  puts ("PASS dns-manager");
  return 0;
}
