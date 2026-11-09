/* Generated, standalone MuLIB add-on symbol and API test: uip-icmp */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_ICMP
#include "mulib-uip-icmp.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_icmp_echo;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_icmp_verify;
    assert (p);
  }

  uint8_t f[32];
  size_t n = 0;
  assert (!mulib_uip_icmp_echo (f, sizeof f, 0, 12, 3, "hi", 2, &n));
  assert (!mulib_uip_icmp_verify (f, n));
  puts ("PASS uip-icmp");
  return 0;
}
