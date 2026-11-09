#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NFNETLINK
#include "mulib-nfnetlink.h"
#include <stdio.h>
int main(void) { mulib_netlink_socket_t s = MULIB_NETLINK_INIT; printf("netfilter socket status=%d\n", mulib_nfnetlink_open(&s)); mulib_netlink_close(&s); return 0; }
