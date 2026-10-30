/** @file mulib-filectl.h
 * @brief POSIX descriptor flags and advisory whole-file record locks.
 * Define MULIB_IMPL_FILECTL once. Descriptors are always borrowed.
 */
#ifndef MULIB_FILECTL_H
#define MULIB_FILECTL_H
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_FILECTL_OK = 0,
    MULIB_FILECTL_BUSY = 1,
    MULIB_FILECTL_EINVAL = -1,
    MULIB_FILECTL_EIO = -2,
    MULIB_FILECTL_ENOTSUP = -3
  };
  /** @brief Toggle only O_NONBLOCK or FD_CLOEXEC, preserving other flags. */
  int mulib_filectl_nonblocking (int fd, int enabled);
  int mulib_filectl_cloexec (int fd, int enabled);
  /** @brief Nonblocking process-associated fcntl lock: 0 unlock, 1 shared,
   * 2 exclusive. Closing any descriptor for the file can release these locks.
   */
  int mulib_filectl_lock (int fd, int mode);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FILECTL) && !defined(MULIB_FILECTL_IMPLEMENTED)
#define MULIB_FILECTL_IMPLEMENTED
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#endif
int
mulib_filectl_nonblocking (int fd, int on)
{
  if (fd < 0 || (on != 0 && on != 1))
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    int f = fcntl (fd, F_GETFL);
    return f < 0
                   || fcntl (fd, F_SETFL,
                             on ? f | O_NONBLOCK : f & ~O_NONBLOCK)
                          < 0
               ? -2
               : 0;
  }
#else
  return -3;
#endif
}
int
mulib_filectl_cloexec (int fd, int on)
{
  if (fd < 0 || (on != 0 && on != 1))
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    int f = fcntl (fd, F_GETFD);
    return f < 0
                   || fcntl (fd, F_SETFD,
                             on ? f | FD_CLOEXEC : f & ~FD_CLOEXEC)
                          < 0
               ? -2
               : 0;
  }
#else
  return -3;
#endif
}
int
mulib_filectl_lock (int fd, int mode)
{
  if (fd < 0 || mode < 0 || mode > 2)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    struct flock l;
    int r;
    memset (&l, 0, sizeof (l));
    l.l_whence = SEEK_SET;
    l.l_type = mode == 0 ? F_UNLCK : mode == 1 ? F_RDLCK : F_WRLCK;
    do
      {
        r = fcntl (fd, F_SETLK, &l);
      }
    while (r < 0 && errno == EINTR);
    return r == 0 ? 0 : (errno == EACCES || errno == EAGAIN) ? 1 : -2;
  }
#else
  return -3;
#endif
}
#endif
