/* Generated, standalone MuLIB add-on symbol and API test: uip-fragment */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_FRAGMENT
#include "mulib-uip-fragment.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_frag_reset;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_frag_push;
    assert (p);
  }

  mulib_uip_frag_t s = { 0 };
  uint8_t ip[4] = { 1 }, a[8] = { 1 }, b[2] = { 2 };
  size_t n = 0;
  assert (!mulib_uip_frag_push (&s, ip, ip, 17, 9, 8, 0, b, 2, &n));
  assert (mulib_uip_frag_push (&s, ip, ip, 17, 9, 0, 1, a, 8, &n) == 1);
  assert (n == 10);
  puts ("PASS uip-fragment");
  return 0;
}
