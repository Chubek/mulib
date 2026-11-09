#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#include "mulib-rtnl.h"
#include <stdio.h>
static int visit(const void *p, size_t n, void *ctx) { (void)p; (void)n; ++*(unsigned *)ctx; return 0; }
int main(void) { struct ifinfomsg msg = {0}; unsigned count = 0; int r = mulib_rtnl_dump(RTM_GETLINK, &msg, sizeof msg, visit, &count); printf("links=%u status=%d\n", count, r); return 0; }
