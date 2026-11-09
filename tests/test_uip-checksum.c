/* Generated, standalone MuLIB add-on symbol and API test: uip-checksum */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_CHECKSUM
#include "mulib-uip-checksum.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_checksum;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_checksum_ipv4;
    assert (p);
  }

  uint8_t b[] = { 0x45, 0, 0,   20, 0, 0, 0,   0, 64, 1,
                  0,    0, 127, 0,  0, 1, 127, 0, 0,  1 };
  uint16_t v = mulib_uip_checksum (b, sizeof b);
  b[10] = (uint8_t)(v >> 8);
  b[11] = (uint8_t)v;
  assert (mulib_uip_checksum (b, sizeof b) == 0);
  puts ("PASS uip-checksum");
  return 0;
}
