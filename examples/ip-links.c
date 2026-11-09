#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_LINK
#include "mulib-rtnl-link.h"
#include <stdio.h>
#include <linux/rtnetlink.h>
static int
visit (const void *raw, size_t n, void *unused)
{
  (void)unused;
  const struct nlmsghdr *h = raw;
  if (n < NLMSG_LENGTH (sizeof (struct ifinfomsg)))
    return 0;
  const struct ifinfomsg *i = NLMSG_DATA (h);
  printf ("ifindex=%d flags=0x%x\n", i->ifi_index, i->ifi_flags);
  return 0;
}
int
main (void)
{
  int r = mulib_rtnl_link_dump (visit, NULL);
  if (r)
    fprintf (stderr, "dump failed: %d\n", r);
  return !!r;
}
