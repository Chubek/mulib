/** @file mulib-dbus-properties.h
 * @brief Call D-Bus Properties.Get and decode uint32 or string variant replies
 * C11, single-header, define MULIB_IMPL_DBUS_PROPERTIES exactly once.
 */
#ifndef MULIB_DBUS_PROPERTIES_H
#define MULIB_DBUS_PROPERTIES_H
#include "mulib-dbus-client.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_dbus_properties_u32 (mulib_dbus_client_t *client,
                                 const char *service, const char *path,
                                 const char *interface, const char *property,
                                 uint32_t *value);
  int mulib_dbus_properties_string (mulib_dbus_client_t *client,
                                    const char *service, const char *path,
                                    const char *interface,
                                    const char *property, char *out,
                                    size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBUS_PROPERTIES)                                       \
    && !defined(MULIB_DBUS_PROPERTIES_IMPLEMENTED)
#define MULIB_DBUS_PROPERTIES_IMPLEMENTED
#include <errno.h>
#include <string.h>
static int
mulib_dbus_properties_extract_ (const uint8_t *p, size_t n, char required,
                                void *out, size_t cap)
{
  size_t off = mulib_dbus_client_body_offset (p, n);
  if (!off || n - off < 3 || p[off] != 1 || p[off + 1] != (uint8_t)required
      || p[off + 2] != 0)
    return -EBADMSG;
  uint32_t body = (uint32_t)p[4] | (uint32_t)p[5] << 8 | (uint32_t)p[6] << 16
                  | (uint32_t)p[7] << 24;
  n = off + body;
  if (n - off < 3)
    return -EBADMSG;
  size_t index = (off + 3 + 3) & ~3u;
  if (index + 4 > n)
    return -EBADMSG;
  uint32_t len = (uint32_t)p[index] | (uint32_t)p[index + 1] << 8
                 | (uint32_t)p[index + 2] << 16 | (uint32_t)p[index + 3] << 24;
  if (required == 'u')
    {
      if (cap < 4)
        return -EINVAL;
      memcpy (out, &len, 4);
      return 0;
    }
  if (n - index < 5 || len > n - index - 5)
    return -EBADMSG;
  if (!cap || len > cap - 1)
    return -ENOSPC;
  if (p[index + 4 + len] != 0 || memchr (p + index + 4, 0, len))
    return -EBADMSG;
  memcpy (out, p + index + 4, len);
  ((char *)out)[len] = 0;
  return 0;
}
int
mulib_dbus_properties_u32 (mulib_dbus_client_t *c, const char *service,
                           const char *path, const char *iface,
                           const char *prop, uint32_t *out)
{
  if (!out || !iface || !prop)
    return -EINVAL;
  uint8_t response[16384];
  size_t n = 0;
  int r = mulib_dbus_client_call (c, service, path,
                                  "org.freedesktop.DBus.Properties", "Get",
                                  iface, prop, response, sizeof response, &n);
  return r ? r : mulib_dbus_properties_extract_ (response, n, 'u', out, 4);
}
int
mulib_dbus_properties_string (mulib_dbus_client_t *c, const char *service,
                              const char *path, const char *iface,
                              const char *prop, char *out, size_t cap)
{
  if (!out || !cap || !iface || !prop)
    return -EINVAL;
  uint8_t response[16384];
  size_t n = 0;
  int r = mulib_dbus_client_call (c, service, path,
                                  "org.freedesktop.DBus.Properties", "Get",
                                  iface, prop, response, sizeof response, &n);
  return r ? r : mulib_dbus_properties_extract_ (response, n, 's', out, cap);
}
#endif
