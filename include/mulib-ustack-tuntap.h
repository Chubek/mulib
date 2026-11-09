/** @file mulib-ustack-tuntap.h
 * @brief Drive userspace Ethernet stack using an existing Linux TAP descriptor
 * C11, single-header, define MULIB_IMPL_USTACK_TUNTAP exactly once.
 */
#ifndef MULIB_USTACK_TUNTAP_H
#define MULIB_USTACK_TUNTAP_H
#include "mulib-ustack.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_ustack_tap_once (int tap_fd, mulib_ustack_t *stack);
  int mulib_ustack_tap_emit (const void *frame, size_t n, void *context);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_USTACK_TUNTAP)                                         \
    && !defined(MULIB_USTACK_TUNTAP_IMPLEMENTED)
#define MULIB_USTACK_TUNTAP_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <unistd.h>
#endif
int
mulib_ustack_tap_emit (const void *frame, size_t n, void *ctx)
{
  if (!ctx || (!frame && n))
    return -EINVAL;
#ifdef __linux__
  int fd = *(int *)ctx;
  ssize_t k = write (fd, frame, n);
  return k < 0 ? -errno : (size_t)k == n ? 0 : -EIO;
#else
  return -ENOSYS;
#endif
}
int
mulib_ustack_tap_once (int fd, mulib_ustack_t *s)
{
  if (fd < 0 || !s)
    return -EINVAL;
#ifdef __linux__
  uint8_t frame[2048];
  ssize_t n = read (fd, frame, sizeof frame);
  if (n < 0)
    return -errno;
  if (!n)
    return -ENODATA;
  return mulib_ustack_process (s, frame, (size_t)n);
#else
  return -ENOSYS;
#endif
}
#endif
