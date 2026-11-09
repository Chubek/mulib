#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_ROUTE
#include "mulib-rtnl-route.h"
#include <stdio.h>
#include <linux/rtnetlink.h>
static int
each (const void *raw, size_t n, void *ctx)
{
  (void)ctx;
  const struct nlmsghdr *h = raw;
  if (n < NLMSG_LENGTH (sizeof (struct rtmsg)))
    return 0;
  const struct rtmsg *m = NLMSG_DATA (h);
  printf ("family %u prefix %u table %u\n", m->rtm_family, m->rtm_dst_len,
          m->rtm_table);
  return 0;
}
int
main (void)
{
  int r = mulib_rtnl_route_dump (4, each, NULL);
  if (r)
    fprintf (stderr, "route dump error %d\n", r);
  return !!r;
}
