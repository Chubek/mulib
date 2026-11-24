/** @file mulib-rtnl-link.h
 * @brief Inspect and administratively bring interfaces up/down via RTM_NEWLINK
 * C11, single-header, define MULIB_IMPL_RTNL_LINK exactly once.
 */
#ifndef MULIB_RTNL_LINK_H
#define MULIB_RTNL_LINK_H
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_link_index (const char *name);
  int mulib_rtnl_link_set_up (unsigned index, int up);
  int mulib_rtnl_link_dump (mulib_netlink_visit_fn fn, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_LINK) && !defined(MULIB_RTNL_LINK_IMPLEMENTED)
#define MULIB_RTNL_LINK_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/if.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#endif
int
mulib_rtnl_link_index (const char *name)
{
  if (!name)
    return -EINVAL;
#ifdef __linux__
  unsigned x = if_nametoindex (name);
  return x ? (int)x : -ENODEV;
#else
  return -ENOSYS;
#endif
}
int
mulib_rtnl_link_set_up (unsigned idx, int up)
{
  if (!idx || (up != 0 && up != 1))
    return -EINVAL;
#ifdef __linux__
  struct ifinfomsg m = { 0 };
  m.ifi_family = AF_UNSPEC;
  m.ifi_index = (int)idx;
  m.ifi_change = IFF_UP;
  if (up)
    m.ifi_flags = IFF_UP;
  return mulib_rtnl_request (RTM_NEWLINK, NLM_F_ACK, &m, sizeof m, NULL, 0,
                             NULL, NULL);
#else
  return -ENOSYS;
#endif
}
int
mulib_rtnl_link_dump (mulib_netlink_visit_fn fn, void *ctx)
{
#ifdef __linux__
  struct ifinfomsg msg = { 0 };
  msg.ifi_family = AF_UNSPEC;
  return mulib_rtnl_dump (RTM_GETLINK, &msg, sizeof msg, fn, ctx);
#else
  return -ENOSYS;
#endif
}
#endif
