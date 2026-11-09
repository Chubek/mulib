/** @file mulib-rtnl-address.h
 * @brief Enumerate or assign IPv4/IPv6 interface addresses over rtnetlink
 * C11, single-header, define MULIB_IMPL_RTNL_ADDRESS exactly once.
 */
#ifndef MULIB_RTNL_ADDRESS_H
#define MULIB_RTNL_ADDRESS_H
#include "mulib-netlink-attr.h"
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_address_dump (unsigned family, mulib_netlink_visit_fn cb,
                               void *ctx);
  int mulib_rtnl_address_change (unsigned index, unsigned family,
                                 const void *address, unsigned prefix,
                                 int add);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_ADDRESS)                                          \
    && !defined(MULIB_RTNL_ADDRESS_IMPLEMENTED)
#define MULIB_RTNL_ADDRESS_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#endif
int
mulib_rtnl_address_dump (unsigned family, mulib_netlink_visit_fn cb, void *ctx)
{
  if (family != 0 && family != 4 && family != 6)
    return -EINVAL;
#ifdef __linux__
  struct ifaddrmsg a = { 0 };
  a.ifa_family = (uint8_t)(family == 4   ? AF_INET
                           : family == 6 ? AF_INET6
                                         : AF_UNSPEC);
  return mulib_rtnl_dump (RTM_GETADDR, &a, sizeof a, cb, ctx);
#else
  return -ENOSYS;
#endif
}
int
mulib_rtnl_address_change (unsigned idx, unsigned family, const void *addr,
                           unsigned prefix, int add)
{
  if (!idx || !addr || (family != 4 && family != 6)
      || prefix > (family == 4 ? 32u : 128u) || (add != 0 && add != 1))
    return -EINVAL;
#ifdef __linux__
  struct ifaddrmsg a = { 0 };
  a.ifa_family = (uint8_t)(family == 4 ? AF_INET : AF_INET6);
  a.ifa_prefixlen = (uint8_t)prefix;
  a.ifa_index = idx;
  a.ifa_scope = RT_SCOPE_UNIVERSE;
  unsigned char attrs[64] = { 0 };
  size_t used = 0;
  int r = mulib_netlink_attr_append (attrs, sizeof attrs, &used, IFA_LOCAL,
                                     addr, family == 4 ? 4 : 16);
  if (r)
    return r;
  return mulib_rtnl_request (
      add ? RTM_NEWADDR : RTM_DELADDR,
      (uint16_t)(NLM_F_ACK | (add ? (NLM_F_CREATE | NLM_F_EXCL) : 0)), &a,
      sizeof a, attrs, used, NULL, NULL);
#else
  return -ENOSYS;
#endif
}
#endif
