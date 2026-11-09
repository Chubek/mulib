/* Queue binding changes packet delivery; only validate an invalid owner. */
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NFNETLINK
#define MULIB_IMPL_NETFILTER_QUEUE
#include "mulib-netfilter-queue.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_netfilter_queue_bind (NULL, 0) == -EINVAL);
  return 0;
}
