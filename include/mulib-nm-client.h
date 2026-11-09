/** @file mulib-nm-client.h
 * @brief Read NetworkManager global state and device object-path reply
 * C11, single-header, define MULIB_IMPL_NM_CLIENT exactly once.
 */
#ifndef MULIB_NM_CLIENT_H
#define MULIB_NM_CLIENT_H
#include "mulib-dbus-properties.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_nm_client_state (mulib_dbus_client_t *c, uint32_t *state);
  int mulib_nm_client_devices (mulib_dbus_client_t *c, void *reply, size_t cap,
                               size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NM_CLIENT) && !defined(MULIB_NM_CLIENT_IMPLEMENTED)
#define MULIB_NM_CLIENT_IMPLEMENTED
#include <errno.h>
int
mulib_nm_client_state (mulib_dbus_client_t *c, uint32_t *out)
{
  return mulib_dbus_properties_u32 (
      c, "org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager",
      "org.freedesktop.NetworkManager", "State", out);
}
int
mulib_nm_client_devices (mulib_dbus_client_t *c, void *reply, size_t cap,
                         size_t *used)
{
  return mulib_dbus_client_call (c, "org.freedesktop.NetworkManager",
                                 "/org/freedesktop/NetworkManager",
                                 "org.freedesktop.NetworkManager",
                                 "GetDevices", NULL, NULL, reply, cap, used);
}
#endif
