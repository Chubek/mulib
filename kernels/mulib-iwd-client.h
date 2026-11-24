/** @file mulib-iwd-client.h
 * @brief Read iwd object hierarchy through ObjectManager.GetManagedObjects
 * C11, single-header, define MULIB_IMPL_IWD_CLIENT exactly once.
 */
#ifndef MULIB_IWD_CLIENT_H
#define MULIB_IWD_CLIENT_H
#include "mulib-dbus-client.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_iwd_client_objects (mulib_dbus_client_t *c, void *reply,
                                size_t cap, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IWD_CLIENT) && !defined(MULIB_IWD_CLIENT_IMPLEMENTED)
#define MULIB_IWD_CLIENT_IMPLEMENTED
#include <errno.h>
int
mulib_iwd_client_objects (mulib_dbus_client_t *c, void *buf, size_t cap,
                          size_t *used)
{
  return mulib_dbus_client_call (
      c, "net.connman.iwd", "/", "org.freedesktop.DBus.ObjectManager",
      "GetManagedObjects", NULL, NULL, buf, cap, used);
}
#endif
