/** @file mulib-wifi-station.h
 * @brief Request wireless station information from nl80211
 * (NL80211_CMD_GET_STATION) C11, single-header, define MULIB_IMPL_WIFI_STATION
 * exactly once.
 */
#ifndef MULIB_WIFI_STATION_H
#define MULIB_WIFI_STATION_H
#include "mulib-nl80211.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_wifi_station_query (unsigned ifindex, mulib_netlink_visit_fn fn,
                                void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WIFI_STATION)                                          \
    && !defined(MULIB_WIFI_STATION_IMPLEMENTED)
#define MULIB_WIFI_STATION_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/netlink.h>
#include <linux/nl80211.h>
#endif
int
mulib_wifi_station_query (unsigned ifindex, mulib_netlink_visit_fn fn,
                          void *ctx)
{
  if (!ifindex)
    return -EINVAL;
#ifdef __linux__
  return mulib_nl80211_command (NL80211_CMD_GET_STATION, ifindex, NLM_F_DUMP,
                                fn, ctx);
#else
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
