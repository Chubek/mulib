/** @file mulib-datagram.h
 * @brief IPv4/IPv6 UDP sockets with explicit truncation and address metadata.
 * Define MULIB_IMPL_DATAGRAM and MULIB_IMPL_FILECTL once.
 */
#ifndef MULIB_DATAGRAM_H
#define MULIB_DATAGRAM_H
#include "mulib-filectl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_datagram_address
  {
    unsigned char bytes[16];
    uint16_t port;
    unsigned family; /* 4 or 6 */
    uint32_t scope;
  } mulib_datagram_address_t;
  typedef struct mulib_datagram
  {
    int fd;
  } mulib_datagram_t;
#define MULIB_DATAGRAM_INIT { -1 }
  enum
  {
    MULIB_DATAGRAM_OK = 0,
    MULIB_DATAGRAM_AGAIN = 1,
    MULIB_DATAGRAM_TRUNCATED = 2,
    MULIB_DATAGRAM_EINVAL = -1,
    MULIB_DATAGRAM_EIO = -2,
    MULIB_DATAGRAM_ENOTSUP = -3
  };
  /** @brief Bind a new owning socket; port zero selects an ephemeral port.
   * On success local receives the bound address. Output must have fd == -1. */
  int mulib_datagram_open (mulib_datagram_t *,
                           const mulib_datagram_address_t *, int nonblocking,
                           mulib_datagram_address_t *local);
  /** @brief Send an entire datagram (including zero bytes); 65507 byte
   * maximum. */
  int mulib_datagram_send (mulib_datagram_t *,
                           const mulib_datagram_address_t *, const void *,
                           size_t);
  /** @brief Receive one datagram. Truncation consumes it and returns the
   * copied prefix length. Empty datagrams are OK, never EOF. */
  int mulib_datagram_receive (mulib_datagram_t *, mulib_datagram_address_t *,
                              void *, size_t, size_t *);
  /** @brief Close once and reset fd to -1. */
  int mulib_datagram_close (mulib_datagram_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DATAGRAM) && !defined(MULIB_DATAGRAM_IMPLEMENTED)
#define MULIB_DATAGRAM_IMPLEMENTED
#include <errno.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
static socklen_t
mulib_datagram_native_ (const mulib_datagram_address_t *a,
                        struct sockaddr_storage *s)
{
  memset (s, 0, sizeof (*s));
  if (a->family == 4)
    {
      struct sockaddr_in *v = (struct sockaddr_in *)s;
      v->sin_family = AF_INET;
      v->sin_port = htons (a->port);
      memcpy (&v->sin_addr, a->bytes, 4);
      return sizeof (*v);
    }
  else
    {
      struct sockaddr_in6 *v = (struct sockaddr_in6 *)s;
      v->sin6_family = AF_INET6;
      v->sin6_port = htons (a->port);
      v->sin6_scope_id = a->scope;
      memcpy (&v->sin6_addr, a->bytes, 16);
      return sizeof (*v);
    }
}
static void
mulib_datagram_address_ (const struct sockaddr_storage *s,
                         mulib_datagram_address_t *a)
{
  memset (a, 0, sizeof (*a));
  if (s->ss_family == AF_INET)
    {
      const struct sockaddr_in *v = (const struct sockaddr_in *)s;
      a->family = 4;
      a->port = ntohs (v->sin_port);
      memcpy (a->bytes, &v->sin_addr, 4);
    }
  else
    {
      const struct sockaddr_in6 *v = (const struct sockaddr_in6 *)s;
      a->family = 6;
      a->port = ntohs (v->sin6_port);
      a->scope = v->sin6_scope_id;
      memcpy (a->bytes, &v->sin6_addr, 16);
    }
}
#endif
int
mulib_datagram_open (mulib_datagram_t *s, const mulib_datagram_address_t *a,
                     int nb, mulib_datagram_address_t *local)
{
  if (!s || s->fd != -1 || !a || !local || (a->family != 4 && a->family != 6)
      || (nb != 0 && nb != 1))
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    struct sockaddr_storage addr;
    socklen_t len = mulib_datagram_native_ (a, &addr);
    int fd = socket (a->family == 4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0)
      return -2;
    if (mulib_filectl_cloexec (fd, 1) || mulib_filectl_nonblocking (fd, nb)
        || bind (fd, (struct sockaddr *)&addr, len) < 0
        || getsockname (fd, (struct sockaddr *)&addr, &len) < 0)
      {
        int saved = errno;
        close (fd);
        errno = saved;
        return -2;
      }
    mulib_datagram_address_ (&addr, local);
    s->fd = fd;
    return 0;
  }
#else
  return -3;
#endif
}
int
mulib_datagram_send (mulib_datagram_t *s, const mulib_datagram_address_t *a,
                     const void *p, size_t n)
{
  if (!s || s->fd < 0 || !a || (a->family != 4 && a->family != 6) || (!p && n)
      || n > 65507)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    struct sockaddr_storage addr;
    socklen_t len = mulib_datagram_native_ (a, &addr);
    ssize_t r;
    do
      {
        r = sendto (s->fd, p, n, 0, (struct sockaddr *)&addr, len);
      }
    while (r < 0 && errno == EINTR);
    if (r < 0)
      return errno == EAGAIN || errno == EWOULDBLOCK ? 1 : -2;
    return (size_t)r == n ? 0 : -2;
  }
#else
  return -3;
#endif
}
int
mulib_datagram_receive (mulib_datagram_t *s, mulib_datagram_address_t *a,
                        void *p, size_t n, size_t *count)
{
  if (count)
    *count = 0;
  if (!s || s->fd < 0 || !a || !count || (!p && n))
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    struct sockaddr_storage addr;
    struct msghdr msg;
    struct iovec vec;
    ssize_t r;
    memset (&msg, 0, sizeof (msg));
    memset (&addr, 0, sizeof (addr));
    vec.iov_base = p;
    vec.iov_len = n;
    msg.msg_name = &addr;
    msg.msg_namelen = sizeof (addr);
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;
    do
      {
        r = recvmsg (s->fd, &msg, 0);
      }
    while (r < 0 && errno == EINTR);
    if (r < 0)
      return errno == EAGAIN || errno == EWOULDBLOCK ? 1 : -2;
    mulib_datagram_address_ (&addr, a);
    *count = (size_t)r > n ? n : (size_t)r;
    return msg.msg_flags & MSG_TRUNC ? 2 : 0;
  }
#else
  return -3;
#endif
}
int
mulib_datagram_close (mulib_datagram_t *s)
{
  if (!s)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    int fd = s->fd;
    s->fd = -1;
    return fd < 0 || close (fd) == 0 ? 0 : -2;
  }
#else
  s->fd = -1;
  return -3;
#endif
}
#endif
