/** @file mulib-rtnl-neighbor.h
 * @brief Dump kernel ARP/NDP neighbor entries and add or remove static
 * neighbors C11, single-header, define MULIB_IMPL_RTNL_NEIGHBOR exactly once.
 */
#ifndef MULIB_RTNL_NEIGHBOR_H
#define MULIB_RTNL_NEIGHBOR_H
#include "mulib-netlink-attr.h"
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_neighbor_dump (unsigned family, mulib_netlink_visit_fn fn,
                                void *ctx);
  int mulib_rtnl_neighbor_change (unsigned ifindex, unsigned family,
                                  const void *ip, const uint8_t mac[6],
                                  int add);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_NEIGHBOR)                                         \
    && !defined(MULIB_RTNL_NEIGHBOR_IMPLEMENTED)
#define MULIB_RTNL_NEIGHBOR_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/neighbour.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#endif
int
mulib_rtnl_neighbor_dump (unsigned family, mulib_netlink_visit_fn fn,
                          void *ctx)
{
  if (family != 0 && family != 4 && family != 6)
    return -EINVAL;
#ifdef __linux__
  struct ndmsg m = { 0 };
  m.ndm_family = (uint8_t)(family == 4   ? AF_INET
                           : family == 6 ? AF_INET6
                                         : AF_UNSPEC);
  return mulib_rtnl_dump (RTM_GETNEIGH, &m, sizeof m, fn, ctx);
#else
  return -ENOSYS;
#endif
}
int
mulib_rtnl_neighbor_change (unsigned idx, unsigned family, const void *ip,
                            const uint8_t mac[6], int add)
{
  if (!idx || !ip || (family != 4 && family != 6) || (add && !mac)
      || (add != 0 && add != 1))
    return -EINVAL;
#ifdef __linux__
  struct ndmsg m = { 0 };
  m.ndm_family = (uint8_t)(family == 4 ? AF_INET : AF_INET6);
  m.ndm_ifindex = (int)idx;
  m.ndm_state = NUD_PERMANENT;
  unsigned char attrs[80];
  size_t used = 0;
  int r = mulib_netlink_attr_append (attrs, sizeof attrs, &used, NDA_DST, ip,
                                     family == 4 ? 4 : 16);
  if (r)
    return r;
  if (add)
    {
      r = mulib_netlink_attr_append (attrs, sizeof attrs, &used, NDA_LLADDR,
                                     mac, 6);
      if (r)
        return r;
    }
  return mulib_rtnl_request (
      add ? RTM_NEWNEIGH : RTM_DELNEIGH,
      (uint16_t)(NLM_F_ACK | (add ? (NLM_F_CREATE | NLM_F_REPLACE) : 0)), &m,
      sizeof m, attrs, used, NULL, NULL);
#else
  return -ENOSYS;
#endif
}
#endif
