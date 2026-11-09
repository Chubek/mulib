#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_ICMP
#include "mulib-uip-icmp.h"
#include <stdio.h>
int
main (void)
{
  uint8_t packet[128];
  size_t used = 0;
  int r = mulib_uip_icmp_echo (packet, sizeof packet, 0, 42, 3, "MuLIB", 5,
                               &used);
  if (r)
    return 1;
  printf ("Encoded ICMP echo: %zu bytes, checksum valid: %s\n", used,
          mulib_uip_icmp_verify (packet, used) == 0 ? "yes" : "no");
  return 0;
}
