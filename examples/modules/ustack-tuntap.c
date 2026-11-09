/* No TAP device is opened: validate the borrowed descriptor contract. */
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
#include <assert.h>
int
main (void)
{
  mulib_ustack_t stack = { 0 };
  assert (mulib_ustack_tap_once (-1, &stack) == -EINVAL);
  return 0;
}
