#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_SOCK_DIAG
#include "mulib-sock-diag.h"
#include <stdio.h>
static int
visit (const void *p, size_t n, void *ctx)
{
  (void)p;
  (void)n;
  ++*(unsigned *)ctx;
  return 0;
}
int
main (void)
{
  unsigned count = 0;
  int r = mulib_sock_diag_dump (4, 6, visit, &count);
  printf ("TCP sockets=%u status=%d\n", count, r);
  return 0;
}
