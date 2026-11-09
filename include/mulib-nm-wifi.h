/** @file mulib-nm-wifi.h
 * @brief Query NetworkManager Wi-Fi access point object paths as raw D-Bus
 * reply C11, single-header, define MULIB_IMPL_NM_WIFI exactly once.
 */
#ifndef MULIB_NM_WIFI_H
#define MULIB_NM_WIFI_H
#include "mulib-dbus-client.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_nm_wifi_access_points (mulib_dbus_client_t *c,
                                   const char *wireless_device_path,
                                   void *reply, size_t cap, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NM_WIFI) && !defined(MULIB_NM_WIFI_IMPLEMENTED)
#define MULIB_NM_WIFI_IMPLEMENTED
#include <errno.h>
int
mulib_nm_wifi_access_points (mulib_dbus_client_t *c, const char *path,
                             void *buf, size_t cap, size_t *used)
{
  return mulib_dbus_client_call (
      c, "org.freedesktop.NetworkManager", path,
      "org.freedesktop.NetworkManager.Device.Wireless", "GetAllAccessPoints",
      NULL, NULL, buf, cap, used);
}
#endif
