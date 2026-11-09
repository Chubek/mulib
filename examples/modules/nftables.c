#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NFNETLINK
#define MULIB_IMPL_NFTABLES
#include "mulib-nftables.h"
#include <stdio.h>
static int visit(const void *p, size_t n, void *ctx) { (void)p; (void)n; ++*(unsigned *)ctx; return 0; }
int main(void) { unsigned count = 0; int r = mulib_nftables_tables(visit, &count); printf("tables=%u status=%d (CAP_NET_ADMIN may be required)\n", count, r); return 0; }
