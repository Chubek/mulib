/** @file mulib-rtnl-route.h
 * @brief Query IPv4/IPv6 routing tables and add or remove an ordinary unicast
 * route C11, single-header, define MULIB_IMPL_RTNL_ROUTE exactly once.
 */
#ifndef MULIB_RTNL_ROUTE_H
#define MULIB_RTNL_ROUTE_H
#include "mulib-netlink-attr.h"
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_route_dump (unsigned family, mulib_netlink_visit_fn fn,
                             void *ctx);
  int mulib_rtnl_route_change (unsigned family, const void *dst,
                               unsigned prefix, const void *gateway,
                               unsigned ifindex, uint32_t table, int add);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_ROUTE) && !defined(MULIB_RTNL_ROUTE_IMPLEMENTED)
#define MULIB_RTNL_ROUTE_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#endif
int
mulib_rtnl_route_dump (unsigned family, mulib_netlink_visit_fn fn, void *ctx)
{
  if (family != 0 && family != 4 && family != 6)
    return -EINVAL;
#ifdef __linux__
  struct rtmsg m = { 0 };
  m.rtm_family = (uint8_t)(family == 4   ? AF_INET
                           : family == 6 ? AF_INET6
                                         : AF_UNSPEC);
  return mulib_rtnl_dump (RTM_GETROUTE, &m, sizeof m, fn, ctx);
#else
  return -ENOSYS;
#endif
}
int
mulib_rtnl_route_change (unsigned fam, const void *dst, unsigned prefix,
                         const void *gw, unsigned ifindex, uint32_t table,
                         int add)
{
  if ((fam != 4 && fam != 6) || (!dst && prefix)
      || prefix > (fam == 4 ? 32u : 128u) || (!gw && !ifindex)
      || (add != 0 && add != 1))
    return -EINVAL;
#ifdef __linux__
  struct rtmsg m = { 0 };
  m.rtm_family = (uint8_t)(fam == 4 ? AF_INET : AF_INET6);
  m.rtm_dst_len = prefix;
  m.rtm_table = table <= 255 ? (uint8_t)table : RT_TABLE_UNSPEC;
  m.rtm_protocol = RTPROT_STATIC;
  m.rtm_scope = gw ? RT_SCOPE_UNIVERSE : RT_SCOPE_LINK;
  m.rtm_type = RTN_UNICAST;
  unsigned char attrs[128];
  size_t len = 0;
  int r;
  size_t width = fam == 4 ? 4 : 16;
  if (prefix)
    {
      r = mulib_netlink_attr_append (attrs, sizeof attrs, &len, RTA_DST, dst,
                                     width);
      if (r)
        return r;
    }
  if (gw)
    {
      r = mulib_netlink_attr_append (attrs, sizeof attrs, &len, RTA_GATEWAY,
                                     gw, width);
      if (r)
        return r;
    }
  if (ifindex)
    {
      r = mulib_netlink_attr_append (attrs, sizeof attrs, &len, RTA_OIF,
                                     &ifindex, 4);
      if (r)
        return r;
    }
  if (table > 255)
    {
      r = mulib_netlink_attr_append (attrs, sizeof attrs, &len, RTA_TABLE,
                                     &table, 4);
      if (r)
        return r;
    }
  return mulib_rtnl_request (
      add ? RTM_NEWROUTE : RTM_DELROUTE,
      (uint16_t)(NLM_F_ACK | (add ? (NLM_F_CREATE | NLM_F_EXCL) : 0)), &m,
      sizeof m, attrs, len, NULL, NULL);
#else
  (void)dst;
  (void)gw;
  (void)ifindex;
  (void)table;
  return -ENOSYS;
#endif
}
#endif
