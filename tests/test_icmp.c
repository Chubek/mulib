/* Generated, standalone MuLIB add-on symbol and API test: icmp */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_ICMP
#include "mulib-icmp.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_icmp_checksum;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_icmp_echo;
    assert (p);
  }

  uint8_t b[16];
  size_t n = 0;
  assert (!mulib_icmp_echo (b, sizeof b, 8, 1, 2, NULL, 0, &n));
  assert (n == 8);
  assert (mulib_icmp_checksum (b, n) == 0);
  puts ("PASS icmp");
  return 0;
}
