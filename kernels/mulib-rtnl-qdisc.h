/** @file mulib-rtnl-qdisc.h
 * @brief Dump kernel qdisc objects via RTM_GETQDISC
 * C11, single-header, define MULIB_IMPL_RTNL_QDISC exactly once.
 */
#ifndef MULIB_RTNL_QDISC_H
#define MULIB_RTNL_QDISC_H
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_qdisc_dump (unsigned ifindex, mulib_netlink_visit_fn fn,
                             void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_QDISC) && !defined(MULIB_RTNL_QDISC_IMPLEMENTED)
#define MULIB_RTNL_QDISC_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/rtnetlink.h>
#include <netinet/in.h>

#endif
int
mulib_rtnl_qdisc_dump (unsigned ifindex, mulib_netlink_visit_fn fn, void *ctx)
{
  if (!ifindex)
    return -EINVAL;
#ifdef __linux__
  struct tcmsg m = { 0 };
  m.tcm_family = AF_UNSPEC;
  m.tcm_ifindex = (int)ifindex;
  return mulib_rtnl_dump (RTM_GETQDISC, &m, sizeof m, fn, ctx);
#else
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
