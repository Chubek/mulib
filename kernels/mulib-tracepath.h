/** @file mulib-tracepath.h
 * @brief Single-hop UDP TTL probe with Linux IP_RECVERR error-queue response
 * C11, single-header, define MULIB_IMPL_TRACEPATH exactly once.
 */
#ifndef MULIB_TRACEPATH_H
#define MULIB_TRACEPATH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_tracepath_probe (const uint8_t ipv4[4], uint16_t port, int ttl,
                             int timeout_ms, int *icmp_type, int *icmp_code);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TRACEPATH) && !defined(MULIB_TRACEPATH_IMPLEMENTED)
#define MULIB_TRACEPATH_IMPLEMENTED
#include <errno.h>
#include <string.h>
#include <time.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <linux/errqueue.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
int
mulib_tracepath_probe (const uint8_t ip[4], uint16_t port, int ttl,
                       int timeout, int *typ, int *code)
{
  if (!ip || !port || ttl < 1 || ttl > 255 || timeout < 0 || !typ || !code)
    return -EINVAL;
#ifdef __linux__
  int fd = socket (AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -errno;
  int one = 1;
  if (setsockopt (fd, SOL_IP, IP_RECVERR, &one, sizeof one) < 0
      || setsockopt (fd, SOL_IP, IP_TTL, &ttl, sizeof ttl) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  struct sockaddr_in dst = { 0 };
  dst.sin_family = AF_INET;
  dst.sin_port = htons (port);
  memcpy (&dst.sin_addr, ip, 4);
  char msg = 'x';
  if (sendto (fd, &msg, 1, 0, (void *)&dst, sizeof dst) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  struct pollfd pd = { fd, POLLERR, 0 };
  int r = poll (&pd, 1, timeout);
  if (r <= 0)
    {
      int e = r == 0 ? ETIMEDOUT : errno;
      close (fd);
      return -e;
    }
  char data[128], ctl[512];
  struct iovec iov = { data, sizeof data };
  struct msghdr mh = { 0 };
  mh.msg_iov = &iov;
  mh.msg_iovlen = 1;
  mh.msg_control = ctl;
  mh.msg_controllen = sizeof ctl;
  if (recvmsg (fd, &mh, MSG_ERRQUEUE) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  for (struct cmsghdr *c = CMSG_FIRSTHDR (&mh); c; c = CMSG_NXTHDR (&mh, c))
    if (c->cmsg_level == SOL_IP && c->cmsg_type == IP_RECVERR
        && c->cmsg_len >= CMSG_LEN (sizeof (struct sock_extended_err)))
      {
        const struct sock_extended_err *e = (void *)CMSG_DATA (c);
        *typ = e->ee_type;
        *code = e->ee_code;
        close (fd);
        return 0;
      }
  close (fd);
  return -ENOMSG;
#else
  return -ENOSYS;
#endif
}
#endif
