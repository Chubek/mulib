/** @file mulib-wifi-monitor.h
 * @brief Receive Linux RTMGRP_LINK notifications for wireless device state
 * changes C11, single-header, define MULIB_IMPL_WIFI_MONITOR exactly once.
 */
#ifndef MULIB_WIFI_MONITOR_H
#define MULIB_WIFI_MONITOR_H
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_wifi_monitor_open (mulib_netlink_socket_t *s);
  int mulib_wifi_monitor_next (mulib_netlink_socket_t *s, void *buf,
                               size_t cap, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WIFI_MONITOR)                                          \
    && !defined(MULIB_WIFI_MONITOR_IMPLEMENTED)
#define MULIB_WIFI_MONITOR_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/rtnetlink.h>
#endif
int
mulib_wifi_monitor_open (mulib_netlink_socket_t *s)
{
#ifdef __linux__
  return mulib_netlink_open (s, NETLINK_ROUTE, RTMGRP_LINK);
#else
  return -ENOSYS;
#endif
}
int
mulib_wifi_monitor_next (mulib_netlink_socket_t *s, void *buf, size_t cap,
                         size_t *used)
{
  return mulib_netlink_receive (s, buf, cap, used);
}
#endif
