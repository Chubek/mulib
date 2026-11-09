#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_DEVLINK
#include "mulib-devlink.h"
#include <stdio.h>
int main(void) { uint16_t id = 0; int r = mulib_devlink_family(&id); printf("devlink: status=%d id=%u\n", r, (unsigned)id); return 0; }
