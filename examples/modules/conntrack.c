#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NFNETLINK
#define MULIB_IMPL_CONNTRACK
#include "mulib-conntrack.h"
#include <stdio.h>
static int visit(const void *p, size_t n, void *ctx) { (void)p; (void)n; ++*(unsigned *)ctx; return 0; }
int main(void) { unsigned count = 0; int r = mulib_conntrack_dump(4, visit, &count); printf("entries=%u status=%d (CAP_NET_ADMIN may be required)\n", count, r); return 0; }
