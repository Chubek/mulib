#define MULIB_IMPL_IPV6_RA
#include "mulib-ipv6-ra.h"
#include <assert.h>
int
main (void)
{
  uint8_t packet[48] = { 134 }, prefix[16];
  unsigned bits;
  packet[16] = 3;
  packet[17] = 4;
  packet[18] = 64;
  packet[32] = 0x20;
  assert (!mulib_ipv6_ra_prefix (packet, sizeof packet, prefix, &bits));
  assert (bits == 64 && prefix[0] == 0x20);
  return 0;
}
