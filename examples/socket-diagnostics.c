#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_SOCK_DIAG
#include "mulib-sock-diag.h"
#include <stdio.h>
static int
count (const void *p, size_t n, void *ctx)
{
  (void)p;
  (void)n;
  (*(unsigned *)ctx)++;
  return 0;
}
int
main (void)
{
  unsigned num = 0;
  int r = mulib_sock_diag_dump (4, 6, count, &num);
  printf ("TCP IPv4 sockets: %u (error %d)\n", num, r);
  return 0;
}
