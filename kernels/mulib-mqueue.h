/** @file mulib-mqueue.h
 * @brief System V typed, nonblocking interprocess message queues.
 * Define MULIB_IMPL_MQUEUE once. Queue deletion is explicit, not automatic.
 */
#ifndef MULIB_MQUEUE_H
#define MULIB_MQUEUE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_MQUEUE_OK = 0,
    MULIB_MQUEUE_AGAIN = 1,
    MULIB_MQUEUE_EINVAL = -1,
    MULIB_MQUEUE_EIO = -2,
    MULIB_MQUEUE_ENOTSUP = -3,
    MULIB_MQUEUE_NOMEM = -4,
    MULIB_MQUEUE_TOOSMALL = -5
  };
  /** @brief Create a private mode-0600 queue; id unchanged on failure. */
  int mulib_mqueue_create (int *id);
  /** @brief Nonblocking atomic send, positive type, length <= 1 MiB (OS limits
   * may be smaller). Copies data during call. Zero-length messages supported.
   */
  int mulib_mqueue_send (int id, long type, const void *, size_t);
  /** @brief selector uses msgrcv semantics (zero FIFO, positive exact,
   * negative lowest type <= abs(selector)). Too-small buffers leave the
   * message queued. Count reset on errors; type changed only on success. */
  int mulib_mqueue_receive (int id, long selector, long *type, void *, size_t,
                            size_t *);
  /** @brief Delete a queue and all pending messages. */
  int mulib_mqueue_remove (int id);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MQUEUE) && !defined(MULIB_MQUEUE_IMPLEMENTED)
#define MULIB_MQUEUE_IMPLEMENTED
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/ipc.h>
#include <sys/msg.h>
#endif
int
mulib_mqueue_create (int *id)
{
  if (!id)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    int v = msgget (IPC_PRIVATE, IPC_CREAT | 0600);
    if (v < 0)
      return -2;
    *id = v;
    return 0;
  }
#else
  return -3;
#endif
}
int
mulib_mqueue_send (int id, long type, const void *p, size_t n)
{
  if (id < 0 || type <= 0 || (!p && n) || n > 1048576)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    unsigned char *m = malloc (sizeof (long) + n);
    int r, saved;
    if (!m)
      return -4;
    memcpy (m, &type, sizeof (type));
    if (n)
      memcpy (m + sizeof (long), p, n);
    do
      {
        r = msgsnd (id, m, n, IPC_NOWAIT);
      }
    while (r < 0 && errno == EINTR);
    saved = errno;
    free (m);
    errno = saved;
    return r == 0 ? 0 : errno == EAGAIN ? 1 : -2;
  }
#else
  return -3;
#endif
}
int
mulib_mqueue_receive (int id, long selector, long *type, void *p, size_t cap,
                      size_t *count)
{
  if (count)
    *count = 0;
  if (id < 0 || !type || !count || (!p && cap) || cap > 1048576
      || selector == LONG_MIN)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    unsigned char *m = malloc (sizeof (long) + cap);
    ssize_t r;
    int saved;
    if (!m)
      return -4;
    do
      {
        r = msgrcv (id, m, cap, selector, IPC_NOWAIT);
      }
    while (r < 0 && errno == EINTR);
    saved = errno;
    if (r >= 0)
      {
        memcpy (type, m, sizeof (*type));
        if (r)
          memcpy (p, m + sizeof (long), (size_t)r);
        *count = (size_t)r;
      }
    free (m);
    errno = saved;
    return r >= 0 ? 0 : errno == ENOMSG ? 1 : errno == E2BIG ? -5 : -2;
  }
#else
  return -3;
#endif
}
int
mulib_mqueue_remove (int id)
{
  if (id < 0)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  return msgctl (id, IPC_RMID, NULL) == 0 ? 0 : -2;
#else
  return -3;
#endif
}
#endif
