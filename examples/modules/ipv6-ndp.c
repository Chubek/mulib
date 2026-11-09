#define MULIB_IMPL_IPV6_NDP
#include "mulib-ipv6-ndp.h"
#include <assert.h>
int
main (void)
{
  uint8_t packet[24] = { 135 };
  packet[8] = 0x20;
  mulib_ipv6_ndp_t ndp;
  assert (!mulib_ipv6_ndp_parse (packet, sizeof packet, &ndp));
  assert (ndp.target[0] == 0x20);
  return 0;
}
