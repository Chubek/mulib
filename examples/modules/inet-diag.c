#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_SOCK_DIAG
#define MULIB_IMPL_INET_DIAG
#include "mulib-inet-diag.h"
#include <stdio.h>
static int
visit (const void *p, size_t n, void *ctx)
{
  (void)ctx;
  mulib_inet_diag_socket_t s;
  int r = mulib_inet_diag_decode (p, n, &s);
  if (!r)
    printf ("port=%u state=%u\n", (unsigned)s.src_port, s.state);
  return r;
}
int
main (void)
{
  printf ("dump status=%d\n", mulib_sock_diag_dump (4, 6, visit, NULL));
  return 0;
}
