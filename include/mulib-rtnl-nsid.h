/** @file mulib-rtnl-nsid.h
 * @brief Dump kernel nsid objects via RTM_GETNSID
 * C11, single-header, define MULIB_IMPL_RTNL_NSID exactly once.
 */
#ifndef MULIB_RTNL_NSID_H
#define MULIB_RTNL_NSID_H
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_nsid_dump (mulib_netlink_visit_fn fn, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_NSID) && !defined(MULIB_RTNL_NSID_IMPLEMENTED)
#define MULIB_RTNL_NSID_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/rtnetlink.h>
#include <netinet/in.h>

#endif
int
mulib_rtnl_nsid_dump (mulib_netlink_visit_fn fn, void *ctx)
{
#ifdef __linux__
  struct rtgenmsg m = { 0 };
  m.rtgen_family = AF_UNSPEC;
  return mulib_rtnl_dump (RTM_GETNSID, &m, sizeof m, fn, ctx);
#else
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
