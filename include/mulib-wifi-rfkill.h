/** @file mulib-wifi-rfkill.h
 * @brief Read hardware and software radio block state from /dev/rfkill
 * C11, single-header, define MULIB_IMPL_WIFI_RFKILL exactly once.
 */
#ifndef MULIB_WIFI_RFKILL_H
#define MULIB_WIFI_RFKILL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_wifi_rfkill_state
  {
    uint32_t index;
    uint8_t type, operation, soft, hard;
  } mulib_wifi_rfkill_state_t;
  int mulib_wifi_rfkill_read (int fd, mulib_wifi_rfkill_state_t *out);
  int mulib_wifi_rfkill_open (void);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WIFI_RFKILL) && !defined(MULIB_WIFI_RFKILL_IMPLEMENTED)
#define MULIB_WIFI_RFKILL_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/rfkill.h>
#include <unistd.h>
#ifndef O_CLOEXEC
#define MULIB_WIFI_RFKILL_CLOEXEC_ 02000000
#else
#define MULIB_WIFI_RFKILL_CLOEXEC_ O_CLOEXEC
#endif
#endif
int
mulib_wifi_rfkill_open (void)
{
#ifdef __linux__
  int fd = open ("/dev/rfkill",
                 O_RDONLY | MULIB_WIFI_RFKILL_CLOEXEC_ | O_NONBLOCK);
  return fd < 0 ? -errno : fd;
#else
  return -ENOSYS;
#endif
}
int
mulib_wifi_rfkill_read (int fd, mulib_wifi_rfkill_state_t *out)
{
  if (fd < 0 || !out)
    return -EINVAL;
#ifdef __linux__
  struct rfkill_event e;
  ssize_t n;
  do
    {
      n = read (fd, &e, sizeof e);
    }
  while (n < 0 && errno == EINTR);
  if (n < 0)
    return -errno;
  if ((size_t)n != sizeof e)
    return -EBADMSG;
  mulib_wifi_rfkill_state_t v = { e.idx, e.type, e.op, e.soft, e.hard };
  *out = v;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
