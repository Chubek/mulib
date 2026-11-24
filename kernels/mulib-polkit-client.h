/** @file mulib-polkit-client.h
 * @brief Check whether the PolicyKit D-Bus service is currently registered
 * C11, single-header, define MULIB_IMPL_POLKIT_CLIENT exactly once.
 */
#ifndef MULIB_POLKIT_CLIENT_H
#define MULIB_POLKIT_CLIENT_H
#include "mulib-dbus-client.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_polkit_client_name_has_owner (mulib_dbus_client_t *c,
                                          int *present);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_POLKIT_CLIENT)                                         \
    && !defined(MULIB_POLKIT_CLIENT_IMPLEMENTED)
#define MULIB_POLKIT_CLIENT_IMPLEMENTED
#include <errno.h>
int
mulib_polkit_client_name_has_owner (mulib_dbus_client_t *c, int *out)
{
  if (!out)
    return -EINVAL;
  uint8_t response[4096];
  size_t n = 0;
  int r = mulib_dbus_client_call (
      c, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "NameHasOwner", "org.freedesktop.PolicyKit1",
      NULL, response, sizeof response, &n);
  if (r)
    return r;
  size_t off = mulib_dbus_client_body_offset (response, n);
  if (!off || off + 4 > n || response[4] != 4 || response[5] || response[6]
      || response[7])
    return -EBADMSG;
  uint32_t v = (uint32_t)response[off] | (uint32_t)response[off + 1] << 8
               | (uint32_t)response[off + 2] << 16
               | (uint32_t)response[off + 3] << 24;
  if (v > 1)
    return -EBADMSG;
  *out = (int)v;
  return 0;
}
#endif
