/* Generated, standalone MuLIB add-on symbol and API test: uip-dns */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_DNS
#include "mulib-uip-dns.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_dns_query_a;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_dns_first_a;
    assert (p);
  }

  uint8_t buf[256];
  size_t n = 0;
  assert (!mulib_uip_dns_query_a ("example.com", 42, buf, sizeof buf, &n));
  assert (n > 20 && buf[0] == 0);
  puts ("PASS uip-dns");
  return 0;
}
