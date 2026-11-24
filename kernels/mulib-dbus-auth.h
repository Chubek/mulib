/** @file mulib-dbus-auth.h
 * @brief Authenticate to the system D-Bus socket using EXTERNAL peer
 * credentials C11, single-header, define MULIB_IMPL_DBUS_AUTH exactly once.
 */
#ifndef MULIB_DBUS_AUTH_H
#define MULIB_DBUS_AUTH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_dbus_auth_external (int fd);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBUS_AUTH) && !defined(MULIB_DBUS_AUTH_IMPLEMENTED)
#define MULIB_DBUS_AUTH_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
int
mulib_dbus_auth_external (int fd)
{
  if (fd < 0)
    return -EINVAL;
#ifdef __linux__
  char uid[32];
  int n = snprintf (uid, sizeof uid, "%lu", (unsigned long)geteuid ());
  if (n <= 0 || (size_t)n >= sizeof uid)
    return -EINVAL;
  char req[128];
  size_t j = 0;
  req[j++] = 0;
  const char *tag = "AUTH EXTERNAL ";
  size_t len = strlen (tag);
  memcpy (req + j, tag, len);
  j += len;
  const char *hex = "0123456789ABCDEF";
  for (int i = 0; i < n; i++)
    {
      unsigned char c = (unsigned char)uid[i];
      req[j++] = hex[c >> 4];
      req[j++] = hex[c & 15];
    }
  req[j++] = '\r';
  req[j++] = '\n';
  size_t off = 0;
  while (off < j)
    {
      ssize_t r = send (fd, req + off, j - off, MSG_NOSIGNAL);
      if (r < 0 && errno == EINTR)
        continue;
      if (r <= 0)
        return r < 0 ? -errno : -EPIPE;
      off += (size_t)r;
    }
  char line[256];
  size_t used = 0;
  while (used + 1 < sizeof line)
    {
      ssize_t r = recv (fd, line + used, 1, 0);
      if (r < 0 && errno == EINTR)
        continue;
      if (r <= 0)
        return r < 0 ? -errno : -EPIPE;
      used++;
      if (line[used - 1] == '\n')
        break;
    }
  if (used != 37 || memcmp (line, "OK ", 3) || line[35] != '\r'
      || line[36] != '\n')
    return -EACCES;
  for (size_t i = 3; i < 35; i++)
    if (!((line[i] >= '0' && line[i] <= '9')
          || (line[i] >= 'a' && line[i] <= 'f')
          || (line[i] >= 'A' && line[i] <= 'F')))
      return -EACCES;
  const char begin[] = "BEGIN\r\n";
  off = 0;
  while (off < sizeof begin - 1)
    {
      ssize_t r = send (fd, begin + off, sizeof begin - 1 - off, MSG_NOSIGNAL);
      if (r < 0 && errno == EINTR)
        continue;
      if (r <= 0)
        return r < 0 ? -errno : -EPIPE;
      off += (size_t)r;
    }
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
