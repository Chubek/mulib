/** @file mulib-rtnl-nexthop.h
 * @brief Dump kernel nexthop objects via RTM_GETNEXTHOP
 * C11, single-header, define MULIB_IMPL_RTNL_NEXTHOP exactly once.
 */
#ifndef MULIB_RTNL_NEXTHOP_H
#define MULIB_RTNL_NEXTHOP_H
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_nexthop_dump (unsigned family, mulib_netlink_visit_fn fn,
                               void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_NEXTHOP)                                          \
    && !defined(MULIB_RTNL_NEXTHOP_IMPLEMENTED)
#define MULIB_RTNL_NEXTHOP_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/nexthop.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#endif
int
mulib_rtnl_nexthop_dump (unsigned family, mulib_netlink_visit_fn fn, void *ctx)
{
  if (family != 0 && family != 4 && family != 6)
    return -EINVAL;
#ifdef __linux__
  struct nhmsg m = { 0 };
  m.nh_family = (uint8_t)(family == 4   ? AF_INET
                          : family == 6 ? AF_INET6
                                        : AF_UNSPEC);
  return mulib_rtnl_dump (RTM_GETNEXTHOP, &m, sizeof m, fn, ctx);
#else
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
