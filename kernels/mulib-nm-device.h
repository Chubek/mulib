/** @file mulib-nm-device.h
 * @brief Read a NetworkManager device Interface string via D-Bus
 * Properties.Get C11, single-header, define MULIB_IMPL_NM_DEVICE exactly once.
 */
#ifndef MULIB_NM_DEVICE_H
#define MULIB_NM_DEVICE_H
#include "mulib-dbus-properties.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_nm_device_interface (mulib_dbus_client_t *c,
                                 const char *device_path, char *out,
                                 size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NM_DEVICE) && !defined(MULIB_NM_DEVICE_IMPLEMENTED)
#define MULIB_NM_DEVICE_IMPLEMENTED
#include <errno.h>
int
mulib_nm_device_interface (mulib_dbus_client_t *c, const char *path, char *out,
                           size_t cap)
{
  return mulib_dbus_properties_string (
      c, "org.freedesktop.NetworkManager", path,
      "org.freedesktop.NetworkManager.Device", "Interface", out, cap);
}
#endif
