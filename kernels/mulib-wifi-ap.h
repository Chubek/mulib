/** @file mulib-wifi-ap.h
 * @brief Request wireless ap information from nl80211
 * (NL80211_CMD_GET_INTERFACE) C11, single-header, define MULIB_IMPL_WIFI_AP
 * exactly once.
 */
#ifndef MULIB_WIFI_AP_H
#define MULIB_WIFI_AP_H
#include "mulib-nl80211.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_wifi_ap_query (unsigned ifindex, mulib_netlink_visit_fn fn,
                           void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WIFI_AP) && !defined(MULIB_WIFI_AP_IMPLEMENTED)
#define MULIB_WIFI_AP_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/netlink.h>
#include <linux/nl80211.h>
#endif
int
mulib_wifi_ap_query (unsigned ifindex, mulib_netlink_visit_fn fn, void *ctx)
{
  if (!ifindex)
    return -EINVAL;
#ifdef __linux__
  return mulib_nl80211_command (NL80211_CMD_GET_INTERFACE, ifindex, 0, fn,
                                ctx);
#else
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
