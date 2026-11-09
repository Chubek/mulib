/* Generated, standalone MuLIB add-on symbol and API test: ustack */
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
#include "mulib-ustack.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ustack_init;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ustack_process;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ustack_send_udp;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ustack_tick;
    assert (p);
  }
  puts ("PASS ustack");
  return 0;
}
