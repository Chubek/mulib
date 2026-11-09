/* Generated, standalone MuLIB add-on symbol and API test: ustack-tuntap */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_ARP
#define MULIB_IMPL_UIP_ARP_CACHE
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_ETHERNET
#define MULIB_IMPL_UIP_ICMP
#define MULIB_IMPL_UIP_IPV4
#define MULIB_IMPL_UIP_UDP
#define MULIB_IMPL_USTACK
#define MULIB_IMPL_USTACK_TUNTAP
#include "mulib-ustack-tuntap.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ustack_tap_once;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ustack_tap_emit;
    assert (p);
  }

  assert (mulib_ustack_tap_once (-1, NULL) < 0);
  puts ("PASS ustack-tuntap");
  return 0;
}
