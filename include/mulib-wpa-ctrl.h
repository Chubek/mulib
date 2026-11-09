/** @file mulib-wpa-ctrl.h
 * @brief Send a single wpa_supplicant control request over an AF_UNIX datagram
 * socket C11, single-header, define MULIB_IMPL_WPA_CTRL exactly once.
 */
#ifndef MULIB_WPA_CTRL_H
#define MULIB_WPA_CTRL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_wpa_ctrl_request (const char *interface, const char *command,
                              char *response, size_t cap, size_t *got);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WPA_CTRL) && !defined(MULIB_WPA_CTRL_IMPLEMENTED)
#define MULIB_WPA_CTRL_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <stddef.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#endif
int
mulib_wpa_ctrl_request (const char *ifname, const char *cmd, char *out,
                        size_t cap, size_t *got)
{
  if (!ifname || !cmd || !out || !got || !cap || !ifname[0]
      || strlen (ifname) > 15 || !cmd[0] || strlen (cmd) > 4096)
    return -EINVAL;
  *got = 0;
  for (const char *p = ifname; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
          || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.' || *p == '-'))
      return -EINVAL;
#ifdef __linux__
  int fd = socket (AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -errno;
  struct sockaddr_un own = { 0 }, dest = { 0 };
  own.sun_family = dest.sun_family = AF_UNIX;
  struct timespec ts;
  if (timespec_get (&ts, TIME_UTC) != TIME_UTC)
    {
      close (fd);
      return -EIO;
    }
  char name[96];
  int l = snprintf (name, sizeof name, "mulib-wpa-%ld-%ld-%ld",
                    (long)getpid (), (long)ts.tv_sec, ts.tv_nsec);
  if (l <= 0 || (size_t)l >= sizeof own.sun_path)
    {
      close (fd);
      return -ENAMETOOLONG;
    }
  memcpy (own.sun_path + 1, name, (size_t)l);
  if (bind (fd, (void *)&own,
            (socklen_t)(offsetof (struct sockaddr_un, sun_path) + 1 + l))
      < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  char path[sizeof dest.sun_path];
  l = snprintf (path, sizeof path, "/run/wpa_supplicant/%s", ifname);
  if (l <= 0 || (size_t)l >= sizeof dest.sun_path)
    {
      close (fd);
      return -ENAMETOOLONG;
    }
  memcpy (dest.sun_path, path, (size_t)l + 1);
  struct timeval tv = { 2, 0 };
  if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0
      || setsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  if (connect (fd, (void *)&dest,
               (socklen_t)(offsetof (struct sockaddr_un, sun_path) + l + 1))
      < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  ssize_t rv;
  do
    {
      rv = send (fd, cmd, strlen (cmd), 0);
    }
  while (rv < 0 && errno == EINTR);
  if (rv < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  if ((size_t)rv != strlen (cmd))
    {
      close (fd);
      return -EIO;
    }
  do
    {
      rv = recv (fd, out, cap, MSG_TRUNC);
    }
  while (rv < 0 && errno == EINTR);
  int e = errno;
  close (fd);
  if (rv < 0)
    return -e;
  if ((size_t)rv > cap)
    return -EMSGSIZE;
  *got = (size_t)rv;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
