#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_TC
#include "mulib-rtnl-tc.h"
#include <net/if.h>
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
  int r = mulib_rtnl_tc_dump (if_nametoindex ("lo"), visit, &count);
  printf ("filters=%u status=%d\n", count, r);
  return 0;
}
