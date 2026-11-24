/** @file mulib-devlink.h
 * @brief Discover the Linux devlink Generic Netlink family ID
 * C11, single-header, define MULIB_IMPL_DEVLINK exactly once.
 */
#ifndef MULIB_DEVLINK_H
#define MULIB_DEVLINK_H
#include "mulib-netlink-genl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_devlink_family (uint16_t *family_id);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DEVLINK) && !defined(MULIB_DEVLINK_IMPLEMENTED)
#define MULIB_DEVLINK_IMPLEMENTED
#include <errno.h>
int
mulib_devlink_family (uint16_t *out)
{
  if (!out)
    return -EINVAL;
#ifdef __linux__
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  int r = mulib_netlink_open (&s, NETLINK_GENERIC, 0);
  if (r)
    return r;
  r = mulib_netlink_genl_resolve (&s, "devlink", out);
  mulib_netlink_close (&s);
  return r;
#else
  return -ENOSYS;
#endif
}
#endif
