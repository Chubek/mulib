#define MULIB_IMPL_DNS_MANAGER
#include "mulib-dns-manager.h"
#include <stdio.h>
int
main (void)
{
  char ip[128];
  int r
      = mulib_dns_manager_first_nameserver ("/etc/resolv.conf", ip, sizeof ip);
  if (!r)
    puts (ip);
  else
    printf ("resolv.conf status=%d\n", r);
  return 0;
}
