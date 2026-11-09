/* UAPI headers freeze libc-compat decisions before libc's IP declarations.
 * Netfilter modules must remain composable after that ordering, including
 * implementations enabled after an initial declarations-only include. */
#include <linux/if.h>
#include <netinet/in.h>
#include "mulib-nftables.h"
#include "mulib-netfilter-queue.h"

#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NFNETLINK
#define MULIB_IMPL_NFTABLES
#define MULIB_IMPL_NETFILTER_QUEUE
#include "mulib-netlink.h"
#include "mulib-netlink-attr.h"
#include "mulib-nfnetlink.h"
#include "mulib-nftables.h"
#include "mulib-netfilter-queue.h"

#include <assert.h>
int main(void)
{
  /* No network state is changed; compilation is the regression check. */
  assert(mulib_netfilter_queue_bind(NULL, 0) == -EINVAL);
  return 0;
}
