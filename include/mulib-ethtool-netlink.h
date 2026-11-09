/** @file mulib-ethtool-netlink.h
 * @brief Discover the Linux ethtool Generic Netlink family ID
 * C11, single-header, define MULIB_IMPL_ETHTOOL_NETLINK exactly once.
 */
#ifndef MULIB_ETHTOOL_NETLINK_H
#define MULIB_ETHTOOL_NETLINK_H
#include "mulib-netlink-genl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_ethtool_netlink_family (uint16_t *family_id);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ETHTOOL_NETLINK)                                       \
    && !defined(MULIB_ETHTOOL_NETLINK_IMPLEMENTED)
#define MULIB_ETHTOOL_NETLINK_IMPLEMENTED
#include <errno.h>
int
mulib_ethtool_netlink_family (uint16_t *out)
{
  if (!out)
    return -EINVAL;
#ifdef __linux__
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  int r = mulib_netlink_open (&s, NETLINK_GENERIC, 0);
  if (r)
    return r;
  r = mulib_netlink_genl_resolve (&s, "ethtool", out);
  mulib_netlink_close (&s);
  return r;
#else
  return -ENOSYS;
#endif
}
#endif
