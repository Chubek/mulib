/** @file mulib-net-monitor.h
 * @brief Subscribe to link, IPv4/IPv6 address and route multicast Netlink
 * events C11, single-header, define MULIB_IMPL_NET_MONITOR exactly once.
 */
#ifndef MULIB_NET_MONITOR_H
#define MULIB_NET_MONITOR_H
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_net_monitor_open (mulib_netlink_socket_t *sock);
  int mulib_net_monitor_read (mulib_netlink_socket_t *sock, void *buf,
                              size_t cap, size_t *got);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NET_MONITOR) && !defined(MULIB_NET_MONITOR_IMPLEMENTED)
#define MULIB_NET_MONITOR_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/rtnetlink.h>
#endif
int
mulib_net_monitor_open (mulib_netlink_socket_t *s)
{
#ifdef __linux__
  return mulib_netlink_open (s, NETLINK_ROUTE,
                             RTMGRP_LINK | RTMGRP_IPV4_IFADDR
                                 | RTMGRP_IPV6_IFADDR | RTMGRP_IPV4_ROUTE
                                 | RTMGRP_IPV6_ROUTE);
#else
  return -ENOSYS;
#endif
}
int
mulib_net_monitor_read (mulib_netlink_socket_t *s, void *b, size_t n,
                        size_t *got)
{
  return mulib_netlink_receive (s, b, n, got);
}
#endif
