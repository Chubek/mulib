/** @file mulib-socket.h
 * @brief POSIX connected stream descriptor ownership and bounded I/O
 * primitives and numeric IPv4 TCP setup. Define MULIB_IMPL_SOCKET once. No DNS or implicit
 * security. */
#ifndef MULIB_SOCKET_H
#define MULIB_SOCKET_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_socket
  {
    int fd;
  } mulib_socket_t;
#define MULIB_SOCKET_INIT { -1 }
  enum
  {
    MULIB_SOCKET_OK = 0,
    MULIB_SOCKET_AGAIN = 1,
    MULIB_SOCKET_EOF = 2,
    MULIB_SOCKET_EINVAL = -1,
    MULIB_SOCKET_EIO = -2,
    MULIB_SOCKET_ENOTSUP = -3
  };
  /** @brief Create owning close-on-exec AF_UNIX stream pair into two
   * initialized invalid descriptors; 0 success. Outputs unchanged on failure.
   */
  int mulib_socket_pair (mulib_socket_t *a, mulib_socket_t *b);
  /** @brief Blocking TCP connect to a numeric IPv4 address; output must be
   * invalid. Close-on-exec, no DNS. Returns 0, EINVAL/EIO/ENOTSUP below. */
  int mulib_socket_connect (mulib_socket_t *out, const char *address, unsigned port);
  /** @brief Nonblocking numeric IPv4 listener. Port zero requests an ephemeral
   * port, returned through bound_port. No implicit wildcard binding. */
  int mulib_socket_listen (mulib_socket_t *out, const char *address, unsigned port,
                          unsigned *bound_port);
  /** @brief Accept a close-on-exec connected stream; AGAIN when no client. */
  int mulib_socket_accept (mulib_socket_t *listener, mulib_socket_t *out);
  /** @brief Enable/disable O_NONBLOCK; 0 success. */
  int mulib_socket_nonblocking (mulib_socket_t *s, int enabled);
  /** @brief Send once (short writes allowed). EINTR retried, EAGAIN explicit.
   * SIGPIPE suppressed when platform supports MSG_NOSIGNAL/SO_NOSIGPIPE;
   * otherwise ENOTSUP (never silently change process signal policy). Count set
   * zero on errors. */
  int mulib_socket_send (mulib_socket_t *s, const void *data, size_t size,
                         size_t *sent);
  /** @brief Receive once; EOF distinct from AGAIN, zero capacity succeeds with
   * zero. */
  int mulib_socket_receive (mulib_socket_t *s, void *data, size_t capacity,
                            size_t *received);
  /** @brief Close/reset idempotently, 0 success. Never retries close after
   * EINTR. */
  int mulib_socket_close (mulib_socket_t *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SOCKET) && !defined(MULIB_SOCKET_IMPLEMENTED)
#define MULIB_SOCKET_IMPLEMENTED
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define MULIB_SOCKET_POSIX_ 1
#endif
int
mulib_socket_connect (mulib_socket_t *out, const char *address, unsigned port)
{
  if (!out || out->fd != -1 || !address || !port || port > 65535) return -1;
#ifdef MULIB_SOCKET_POSIX_
  {
    struct sockaddr_in a = {0};
    int fd;
    a.sin_family = AF_INET; a.sin_port = htons ((unsigned short)port);
    if (inet_pton (AF_INET, address, &a.sin_addr) != 1) return -1;
    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -2;
    if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0
        || connect (fd, (struct sockaddr *)&a, sizeof a) < 0) { close (fd); return -2; }
    out->fd = fd; return 0;
  }
#else
  return -3;
#endif
}
int
mulib_socket_listen (mulib_socket_t *out, const char *address, unsigned port, unsigned *bound)
{
  if (!out || out->fd != -1 || !address || port > 65535 || !bound) return -1;
#ifdef MULIB_SOCKET_POSIX_
  {
    struct sockaddr_in a = {0};
    socklen_t n = sizeof a;
    int fd, on = 1;
    a.sin_family = AF_INET; a.sin_port = htons ((unsigned short)port);
    if (inet_pton (AF_INET, address, &a.sin_addr) != 1) return -1;
    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -2;
    if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0
        || fcntl (fd, F_SETFL, O_NONBLOCK) < 0
        || setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0
        || bind (fd, (struct sockaddr *)&a, sizeof a) < 0 || listen (fd, 32) < 0
        || getsockname (fd, (struct sockaddr *)&a, &n) < 0) { close (fd); return -2; }
    out->fd = fd; *bound = ntohs (a.sin_port); return 0;
  }
#else
  return -3;
#endif
}
int
mulib_socket_accept (mulib_socket_t *listener, mulib_socket_t *out)
{
  if (!listener || listener->fd < 0 || !out || out->fd != -1 || listener == out) return -1;
#ifdef MULIB_SOCKET_POSIX_
  {
    int fd;
    do { fd = accept (listener->fd, NULL, NULL); } while (fd < 0 && errno == EINTR);
    if (fd < 0) return errno == EAGAIN || errno == EWOULDBLOCK ? 1 : -2;
    if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0) { close (fd); return -2; }
    out->fd = fd; return 0;
  }
#else
  return -3;
#endif
}
int
mulib_socket_pair (mulib_socket_t *a, mulib_socket_t *b)
{
  if (!a || !b || a == b || a->fd != -1 || b->fd != -1)
    return -1;
#ifdef MULIB_SOCKET_POSIX_
  {
    int fd[2], saved;
    if (socketpair (AF_UNIX, SOCK_STREAM, 0, fd) < 0)
      return -2;
    if (fcntl (fd[0], F_SETFD, FD_CLOEXEC) < 0
        || fcntl (fd[1], F_SETFD, FD_CLOEXEC) < 0)
      {
        saved = errno;
        close (fd[0]);
        close (fd[1]);
        errno = saved;
        return -2;
      }
    a->fd = fd[0];
    b->fd = fd[1];
    return 0;
  }
#else
  return -3;
#endif
}
int
mulib_socket_nonblocking (mulib_socket_t *s, int on)
{
  if (!s || s->fd < 0 || (on != 0 && on != 1))
    return -1;
#ifdef MULIB_SOCKET_POSIX_
  {
    int flags = fcntl (s->fd, F_GETFL);
    return flags < 0
                   || fcntl (s->fd, F_SETFL,
                             on ? flags | O_NONBLOCK : flags & ~O_NONBLOCK)
                          < 0
               ? -2
               : 0;
  }
#else
  return -3;
#endif
}
int
mulib_socket_send (mulib_socket_t *s, const void *p, size_t n, size_t *count)
{
  if (count)
    *count = 0;
  if (!s || s->fd < 0 || !count || (!p && n))
    return -1;
#if defined(MULIB_SOCKET_POSIX_)                                              \
    && (defined(MSG_NOSIGNAL) || defined(SO_NOSIGPIPE))
  {
    ssize_t rc;
    int flags = 0;
    if (!n)
      return 0;
    if (n > 1048576)
      n = 1048576;
#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#else
    {
      int on = 1;
      if (setsockopt (s->fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof (on)) < 0)
        return -2;
    }
#endif
    do
      {
        rc = send (s->fd, p, n, flags);
      }
    while (rc < 0 && errno == EINTR);
    if (rc < 0)
      return errno == EAGAIN || errno == EWOULDBLOCK ? 1 : -2;
    *count = (size_t)rc;
    return 0;
  }
#else
  return -3;
#endif
}
int
mulib_socket_receive (mulib_socket_t *s, void *p, size_t n, size_t *count)
{
  if (count)
    *count = 0;
  if (!s || s->fd < 0 || !count || (!p && n))
    return -1;
#ifdef MULIB_SOCKET_POSIX_
  {
    ssize_t rc;
    if (!n)
      return 0;
    if (n > 1048576)
      n = 1048576;
    do
      {
        rc = recv (s->fd, p, n, 0);
      }
    while (rc < 0 && errno == EINTR);
    if (rc < 0)
      return errno == EAGAIN || errno == EWOULDBLOCK ? 1 : -2;
    *count = (size_t)rc;
    return rc ? 0 : 2;
  }
#else
  return -3;
#endif
}
int
mulib_socket_close (mulib_socket_t *s)
{
  if (!s)
    return -1;
  if (s->fd < 0)
    return 0;
#ifdef MULIB_SOCKET_POSIX_
  {
    int fd = s->fd;
    s->fd = -1;
    return close (fd) < 0 ? -2 : 0;
  }
#else
  return -3;
#endif
}
#undef MULIB_SOCKET_POSIX_
#endif
