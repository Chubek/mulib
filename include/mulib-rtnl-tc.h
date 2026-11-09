/** @file mulib-rtnl-tc.h
 * @brief Dump kernel tc objects via RTM_GETTFILTER
 * C11, single-header, define MULIB_IMPL_RTNL_TC exactly once.
 */
#ifndef MULIB_RTNL_TC_H
#define MULIB_RTNL_TC_H
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_tc_dump (unsigned ifindex, mulib_netlink_visit_fn fn,
                          void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_TC) && !defined(MULIB_RTNL_TC_IMPLEMENTED)
#define MULIB_RTNL_TC_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/rtnetlink.h>
#include <netinet/in.h>

#endif
int
mulib_rtnl_tc_dump (unsigned ifindex, mulib_netlink_visit_fn fn, void *ctx)
{
  if (!ifindex)
    return -EINVAL;
#ifdef __linux__
  struct tcmsg m = { 0 };
  m.tcm_family = AF_UNSPEC;
  m.tcm_ifindex = (int)ifindex;
  return mulib_rtnl_dump (RTM_GETTFILTER, &m, sizeof m, fn, ctx);
#else
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
