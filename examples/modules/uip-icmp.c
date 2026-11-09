#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_ICMP
#include "mulib-uip-icmp.h"
#include <assert.h>
int
main (void)
{
  uint8_t bytes[32];
  size_t n = 0;
  assert (!mulib_uip_icmp_echo (bytes, sizeof bytes, 0, 1, 2, "hello", 5, &n));
  assert (!mulib_uip_icmp_verify (bytes, n));
  return 0;
}
