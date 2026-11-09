#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_ROUTE
#include "mulib-rtnl-route.h"
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
  int r = mulib_rtnl_route_dump (4, visit, &count);
  printf ("routes=%u status=%d\n", count, r);
  return 0;
}
