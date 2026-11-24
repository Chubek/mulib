/** @file mulib-nm-secret-agent.h
 * @brief Explicitly unregister a NetworkManager secret agent already
 * registered by the caller C11, single-header, define
 * MULIB_IMPL_NM_SECRET_AGENT exactly once.
 */
#ifndef MULIB_NM_SECRET_AGENT_H
#define MULIB_NM_SECRET_AGENT_H
#include "mulib-dbus-client.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_nm_secret_agent_unregister (mulib_dbus_client_t *c, void *reply,
                                        size_t cap, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NM_SECRET_AGENT)                                       \
    && !defined(MULIB_NM_SECRET_AGENT_IMPLEMENTED)
#define MULIB_NM_SECRET_AGENT_IMPLEMENTED
#include <errno.h>
int
mulib_nm_secret_agent_unregister (mulib_dbus_client_t *c, void *reply,
                                  size_t cap, size_t *used)
{
  return mulib_dbus_client_call (
      c, "org.freedesktop.NetworkManager",
      "/org/freedesktop/NetworkManager/AgentManager",
      "org.freedesktop.NetworkManager.AgentManager", "Unregister", NULL, NULL,
      reply, cap, used);
}
#endif
