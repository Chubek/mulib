#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_UIP_DNS
#include "mulib-uip-dns.h"
#include <stdio.h>
int
main (void)
{
  uint8_t buf[512];
  size_t n = 0;
  if (mulib_uip_dns_query_a ("example.org", 0x1234, buf, sizeof buf, &n))
    return 1;
  printf ("DNS A query encoded: %zu bytes (not sent)\n", n);
  return 0;
}
