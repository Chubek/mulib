/** @file mulib-nm-connection.h
 * @brief List NetworkManager Settings connection object-paths as raw D-Bus
 * frames C11, single-header, define MULIB_IMPL_NM_CONNECTION exactly once.
 */
#ifndef MULIB_NM_CONNECTION_H
#define MULIB_NM_CONNECTION_H
#include "mulib-dbus-client.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_nm_connection_list (mulib_dbus_client_t *c, void *reply,
                                size_t cap, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NM_CONNECTION)                                         \
    && !defined(MULIB_NM_CONNECTION_IMPLEMENTED)
#define MULIB_NM_CONNECTION_IMPLEMENTED
#include <errno.h>
int
mulib_nm_connection_list (mulib_dbus_client_t *c, void *buf, size_t cap,
                          size_t *used)
{
  return mulib_dbus_client_call (c, "org.freedesktop.NetworkManager",
                                 "/org/freedesktop/NetworkManager/Settings",
                                 "org.freedesktop.NetworkManager.Settings",
                                 "ListConnections", NULL, NULL, buf, cap,
                                 used);
}
#endif
