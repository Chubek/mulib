/** @file mulib-resolved-client.h
 * @brief Request systemd-resolved Manager DNS property using raw D-Bus reply
 * C11, single-header, define MULIB_IMPL_RESOLVED_CLIENT exactly once.
 */
#ifndef MULIB_RESOLVED_CLIENT_H
#define MULIB_RESOLVED_CLIENT_H
#include "mulib-dbus-client.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_resolved_client_dns (mulib_dbus_client_t *c, void *reply,
                                 size_t cap, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RESOLVED_CLIENT)                                       \
    && !defined(MULIB_RESOLVED_CLIENT_IMPLEMENTED)
#define MULIB_RESOLVED_CLIENT_IMPLEMENTED
#include <errno.h>
int
mulib_resolved_client_dns (mulib_dbus_client_t *c, void *buf, size_t cap,
                           size_t *used)
{
  return mulib_dbus_client_call (
      c, "org.freedesktop.resolve1", "/org/freedesktop/resolve1",
      "org.freedesktop.DBus.Properties", "Get",
      "org.freedesktop.resolve1.Manager", "DNS", buf, cap, used);
}
#endif
