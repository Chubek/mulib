/** @file mulib-tuntap.h
 * @brief Linux TUN/TAP descriptors with no packet-information prefix.
 * Define MULIB_IMPL_TUNTAP, MULIB_IMPL_ABSTUN, MULIB_IMPL_FILECTL once.
 */
#ifndef MULIB_TUNTAP_H
#define MULIB_TUNTAP_H
#include "mulib-abstun.h"
#include "mulib-filectl.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_tuntap
  {
    int fd;
    size_t mtu;
    char name[16];
  } mulib_tuntap_t;
#define MULIB_TUNTAP_INIT { -1, 0, { 0 } }
  /** @brief Open /dev/net/tun, TUN if tap=0 or TAP if tap=1. requested is an
   * interface name or empty string for kernel selection. Nonblocking/cloexec.
   * mtu is maximum complete packet size (include Ethernet header for TAP).
   * Does not configure addresses/routes/link MTU. 0 success, -1 invalid,
   * -2 OS error (including permissions), -3 unsupported. */
  int mulib_tuntap_open (mulib_tuntap_t *, const char *requested, int tap,
                         size_t mtu);
  /** @brief Return a borrowed abstract tunnel bound to this live object. */
  mulib_abstun_t mulib_tuntap_backend (mulib_tuntap_t *);
  /** @brief Close/reset once. Kernel removes nonpersistent interfaces. */
  int mulib_tuntap_close (mulib_tuntap_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TUNTAP) && !defined(MULIB_TUNTAP_IMPLEMENTED)
#define MULIB_TUNTAP_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
int
mulib_tuntap_open (mulib_tuntap_t *s, const char *name, int tap, size_t mtu)
{
  if (!s || s->fd != -1 || !name || strlen (name) >= 16
      || (tap != 0 && tap != 1) || !mtu || mtu > 65575)
    return -1;
#ifdef __linux__
  {
    struct ifreq req;
    int fd = open ("/dev/net/tun", O_RDWR | O_NONBLOCK);
    if (fd < 0)
      return -2;
    memset (&req, 0, sizeof (req));
    memcpy (req.ifr_name, name, strlen (name));
    req.ifr_flags = (short)((tap ? IFF_TAP : IFF_TUN) | IFF_NO_PI);
    if (mulib_filectl_cloexec (fd, 1) || ioctl (fd, TUNSETIFF, &req) < 0)
      {
        int saved = errno;
        close (fd);
        errno = saved;
        return -2;
      }
    s->fd = fd;
    s->mtu = mtu;
    memcpy (s->name, req.ifr_name, 16);
    s->name[15] = 0;
    return 0;
  }
#else
  return -3;
#endif
}
static int
mulib_tuntap_read_ (void *ctx, void *p, size_t n, size_t *count)
{
  mulib_tuntap_t *s = ctx;
  *count = 0;
  if (!s || s->fd < 0)
    return -1;
#ifdef __linux__
  {
    ssize_t r;
    do
      {
        r = read (s->fd, p, n);
      }
    while (r < 0 && errno == EINTR);
    if (r < 0)
      return errno == EAGAIN || errno == EWOULDBLOCK ? 1 : -2;
    if (!r)
      return -2;
    *count = (size_t)r;
    return 0;
  }
#else
  (void)p;
  (void)n;
  return -3;
#endif
}
static int
mulib_tuntap_write_ (void *ctx, const void *p, size_t n)
{
  mulib_tuntap_t *s = ctx;
  if (!s || s->fd < 0)
    return -1;
#ifdef __linux__
  {
    ssize_t r;
    do
      {
        r = write (s->fd, p, n);
      }
    while (r < 0 && errno == EINTR);
    if (r < 0)
      return errno == EAGAIN || errno == EWOULDBLOCK ? 1 : -2;
    return (size_t)r == n ? 0 : -2;
  }
#else
  (void)p;
  (void)n;
  return -3;
#endif
}
mulib_abstun_t
mulib_tuntap_backend (mulib_tuntap_t *s)
{
  mulib_abstun_t a;
  a.context = s;
  a.mtu = s ? s->mtu : 0;
  a.receive = mulib_tuntap_read_;
  a.send = mulib_tuntap_write_;
  return a;
}
int
mulib_tuntap_close (mulib_tuntap_t *s)
{
  if (!s)
    return -1;
#ifdef __linux__
  {
    int fd = s->fd;
    s->fd = -1;
    s->mtu = 0;
    s->name[0] = 0;
    return fd < 0 || close (fd) == 0 ? 0 : -2;
  }
#else
  return -3;
#endif
}
#endif
