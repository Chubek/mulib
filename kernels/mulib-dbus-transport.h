/** @file mulib-dbus-transport.h
 * @brief Connect to and read/write a framed D-Bus UNIX system-bus connection
 * C11, single-header, define MULIB_IMPL_DBUS_TRANSPORT exactly once.
 */
#ifndef MULIB_DBUS_TRANSPORT_H
#define MULIB_DBUS_TRANSPORT_H
#include "mulib-dbus-auth.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_dbus_transport_connect (const char *path);
  int mulib_dbus_transport_write (int fd, const void *bytes, size_t count);
  int mulib_dbus_transport_read (int fd, void *bytes, size_t capacity,
                                 size_t *count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBUS_TRANSPORT)                                        \
    && !defined(MULIB_DBUS_TRANSPORT_IMPLEMENTED)
#define MULIB_DBUS_TRANSPORT_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <stddef.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#endif
int
mulib_dbus_transport_connect (const char *path)
{
  if (!path)
    path = "/run/dbus/system_bus_socket";
#ifdef __linux__
  if (strlen (path) >= sizeof (((struct sockaddr_un *)0)->sun_path))
    return -ENAMETOOLONG;
  int fd = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -errno;
  struct sockaddr_un addr = { 0 };
  addr.sun_family = AF_UNIX;
  strcpy (addr.sun_path, path);
  if (connect (fd, (void *)&addr,
               (socklen_t)(offsetof (struct sockaddr_un, sun_path)
                           + strlen (path) + 1))
      < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  struct timeval tv = { 3, 0 };
  if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0
      || setsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  int r = mulib_dbus_auth_external (fd);
  if (r)
    {
      close (fd);
      return r;
    }
  return fd;
#else
  (void)path;
  return -ENOSYS;
#endif
}
int
mulib_dbus_transport_write (int fd, const void *p, size_t n)
{
  if (fd < 0 || (!p && n) || n > 1048576)
    return -EINVAL;
#ifdef __linux__
  size_t done = 0;
  while (done < n)
    {
      ssize_t x
          = send (fd, (const unsigned char *)p + done, n - done, MSG_NOSIGNAL);
      if (x < 0 && errno == EINTR)
        continue;
      if (x <= 0)
        return x < 0 ? -errno : -EPIPE;
      done += (size_t)x;
    }
  return 0;
#else
  return -ENOSYS;
#endif
}
static int
mulib_dbus_transport_exact_ (int fd, void *p, size_t n)
{
#ifdef __linux__
  size_t done = 0;
  while (done < n)
    {
      ssize_t r = recv (fd, (uint8_t *)p + done, n - done, 0);
      if (r < 0 && errno == EINTR)
        continue;
      if (r <= 0)
        return r < 0 ? -errno : -ECONNRESET;
      done += (size_t)r;
    }
  return 0;
#else
  return -ENOSYS;
#endif
}
int
mulib_dbus_transport_read (int fd, void *p, size_t cap, size_t *used)
{
  if (fd < 0 || !p || !used || cap < 16)
    return -EINVAL;
  *used = 0;
  uint8_t *b = p;
  int r = mulib_dbus_transport_exact_ (fd, b, 16);
  if (r)
    return r;
  if (b[0] != 'l' || b[3] != 1)
    return -EPROTONOSUPPORT;
  if (b[1] < 1 || b[1] > 4 || !(b[8] | b[9] | b[10] | b[11]))
    return -EBADMSG;
  uint32_t body = (uint32_t)b[4] | (uint32_t)b[5] << 8 | (uint32_t)b[6] << 16
                  | (uint32_t)b[7] << 24;
  uint32_t heads = (uint32_t)b[12] | (uint32_t)b[13] << 8
                   | (uint32_t)b[14] << 16 | (uint32_t)b[15] << 24;
  if (heads > 1048576 || body > 1048576)
    return -EMSGSIZE;
  size_t header = ((size_t)heads + 7u) & ~7u;
  if (header > 1048576 - 16 || body > 1048576 - 16 - header)
    return -EMSGSIZE;
  if (header > cap - 16 || body > cap - 16 - header)
    return -EMSGSIZE;
  r = mulib_dbus_transport_exact_ (fd, b + 16, header + body);
  if (r)
    return r;
  *used = 16 + header + body;
  return 0;
}
#endif
