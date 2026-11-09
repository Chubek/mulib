/** @file mulib-netlink-async.h
 * @brief poll-based bounded readiness and a single pending request completion
 * primitive C11, single-header, define MULIB_IMPL_NETLINK_ASYNC exactly once.
 */
#ifndef MULIB_NETLINK_ASYNC_H
#define MULIB_NETLINK_ASYNC_H
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_netlink_async_wait (int fd, int milliseconds, int writable);
  int mulib_netlink_async_recv (mulib_netlink_socket_t *s, int milliseconds,
                                void *buf, size_t cap, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETLINK_ASYNC)                                         \
    && !defined(MULIB_NETLINK_ASYNC_IMPLEMENTED)
#define MULIB_NETLINK_ASYNC_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <poll.h>
#endif
int
mulib_netlink_async_wait (int fd, int ms, int wr)
{
  if (fd < 0 || ms < -1)
    return -EINVAL;
#ifdef __linux__
  struct pollfd p = { fd, (short)(wr ? POLLOUT : POLLIN), 0 };
  int r;
  r = poll (&p, 1, ms);
  if (r < 0)
    return -errno;
  if (!r)
    return -ETIMEDOUT;
  if (p.revents & POLLNVAL)
    return -EBADF;
  if (p.revents & POLLERR)
    return -EIO;
  if (wr && (p.revents & POLLHUP))
    return -EPIPE;
  if (!(p.revents & p.events))
    return -EPIPE;
  return 0;
#else
  return -ENOSYS;
#endif
}
int
mulib_netlink_async_recv (mulib_netlink_socket_t *s, int ms, void *b, size_t n,
                          size_t *used)
{
  if (!s || !b || n < 16 || !used)
    return -EINVAL;
  *used = 0;
  int r = mulib_netlink_async_wait (s->fd, ms, 0);
  return r ? r : mulib_netlink_receive (s, b, n, used);
}
#endif
