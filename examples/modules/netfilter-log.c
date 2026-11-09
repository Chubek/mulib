/* Do not compete with an existing NFLOG consumer. */
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NFNETLINK
#define MULIB_IMPL_NETFILTER_LOG
#include "mulib-netfilter-log.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_netfilter_log_subscribe (NULL, 0) == -EINVAL);
  return 0;
}
