/** @file mulib-networkd-client.h
 * @brief Read systemd-networkd managed link list as a raw D-Bus reply
 * C11, single-header, define MULIB_IMPL_NETWORKD_CLIENT exactly once.
 */
#ifndef MULIB_NETWORKD_CLIENT_H
#define MULIB_NETWORKD_CLIENT_H
#include "mulib-dbus-client.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_networkd_client_links (mulib_dbus_client_t *c, void *reply,
                                   size_t cap, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETWORKD_CLIENT)                                       \
    && !defined(MULIB_NETWORKD_CLIENT_IMPLEMENTED)
#define MULIB_NETWORKD_CLIENT_IMPLEMENTED
#include <errno.h>
int
mulib_networkd_client_links (mulib_dbus_client_t *c, void *buf, size_t cap,
                             size_t *used)
{
  return mulib_dbus_client_call (c, "org.freedesktop.network1",
                                 "/org/freedesktop/network1",
                                 "org.freedesktop.network1.Manager",
                                 "ListLinks", NULL, NULL, buf, cap, used);
}
#endif
