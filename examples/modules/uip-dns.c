#define MULIB_IMPL_UIP_DNS
#include "mulib-uip-dns.h"
#include <assert.h>
int
main (void)
{
  uint8_t query[256];
  size_t n = 0;
  assert (!mulib_uip_dns_query_a ("example.com", 42, query, sizeof query, &n));
  assert (n == 29);
  return 0;
}
