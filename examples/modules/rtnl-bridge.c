#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_LINK
#define MULIB_IMPL_RTNL_BRIDGE
#include "mulib-rtnl-bridge.h"
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
  int r = mulib_rtnl_bridge_dump (visit, &count);
  printf ("bridges=%u status=%d\n", count, r);
  return 0;
}
