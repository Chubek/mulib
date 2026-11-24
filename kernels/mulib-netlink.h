/** @file mulib-netlink.h
 * @brief Linux NETLINK_ROUTE/Generic Netlink socket, bounded request/response
 * transactions and message iteration C11, single-header, define
 * MULIB_IMPL_NETLINK exactly once.
 */
#ifndef MULIB_NETLINK_H
#define MULIB_NETLINK_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_netlink_socket
  {
    int fd;
    uint32_t pid, seq;
    int protocol;
  } mulib_netlink_socket_t;
#define MULIB_NETLINK_INIT { -1, 0, 0, 0 }
  typedef int (*mulib_netlink_visit_fn) (const void *message, size_t length,
                                         void *ctx);
  int mulib_netlink_open (mulib_netlink_socket_t *s, int protocol,
                          unsigned groups);
  int mulib_netlink_close (mulib_netlink_socket_t *s);
  int mulib_netlink_send (mulib_netlink_socket_t *s, void *msg, size_t n);
  int mulib_netlink_receive (mulib_netlink_socket_t *s, void *out, size_t cap,
                             size_t *got);
  int mulib_netlink_transact (mulib_netlink_socket_t *s, void *msg, size_t n,
                              mulib_netlink_visit_fn visit, void *ctx);
  int mulib_netlink_foreach (const void *data, size_t n,
                             mulib_netlink_visit_fn visit, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETLINK) && !defined(MULIB_NETLINK_IMPLEMENTED)
#define MULIB_NETLINK_IMPLEMENTED
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <linux/netlink.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif
int
mulib_netlink_open (mulib_netlink_socket_t *s, int protocol, unsigned groups)
{
  if (!s || s->fd != -1)
    return -EINVAL;
#ifdef __linux__
  int fd = socket (AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, protocol);
  if (fd < 0)
    return -errno;
  struct sockaddr_nl a = { 0 };
  a.nl_family = AF_NETLINK;
  a.nl_groups = groups;
  if (bind (fd, (void *)&a, sizeof a) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  socklen_t len = sizeof a;
  if (getsockname (fd, (void *)&a, &len) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  struct timeval tv = { 2, 0 };
  if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0)
    {
      int e = errno;
      close (fd);
      return -e;
    }
  s->fd = fd;
  s->pid = a.nl_pid;
  s->seq = 0;
  s->protocol = protocol;
  return 0;
#else
  (void)protocol;
  (void)groups;
  return -ENOSYS;
#endif
}
int
mulib_netlink_close (mulib_netlink_socket_t *s)
{
  if (!s)
    return -EINVAL;
#ifdef __linux__
  if (s->fd >= 0)
    {
      int ret = close (s->fd);
      s->fd = -1;
      return ret < 0 ? -errno : 0;
    }
  return 0;
#else
  return -ENOSYS;
#endif
}
int
mulib_netlink_send (mulib_netlink_socket_t *s, void *msg, size_t n)
{
  if (!s || s->fd < 0 || !msg || n < 16 || n > 65536)
    return -EINVAL;
#ifdef __linux__
  struct nlmsghdr h;
  memcpy (&h, msg, sizeof h);
  if (h.nlmsg_len != n)
    return -EINVAL;
  if (++s->seq == 0)
    ++s->seq;
  h.nlmsg_seq = s->seq;
  h.nlmsg_pid = s->pid;
  memcpy (msg, &h, sizeof h);
  struct sockaddr_nl dest = { 0 };
  dest.nl_family = AF_NETLINK;
  ssize_t rv;
  do
    {
      rv = sendto (s->fd, msg, n, 0, (void *)&dest, sizeof dest);
    }
  while (rv < 0 && errno == EINTR);
  return rv < 0 ? -errno : ((size_t)rv == n ? 0 : -EIO);
#else
  return -ENOSYS;
#endif
}
int
mulib_netlink_receive (mulib_netlink_socket_t *s, void *out, size_t cap,
                       size_t *got)
{
  if (!s || s->fd < 0 || !out || cap < 16 || !got)
    return -EINVAL;
  *got = 0;
#ifdef __linux__
  struct sockaddr_nl src = { 0 };
  struct iovec iov = { out, cap };
  struct msghdr mh = { 0 };
  mh.msg_name = &src;
  mh.msg_namelen = sizeof src;
  mh.msg_iov = &iov;
  mh.msg_iovlen = 1;
  ssize_t n;
  do
    {
      n = recvmsg (s->fd, &mh, 0);
    }
  while (n < 0 && errno == EINTR);
  if (n < 0)
    return -errno;
  if (mh.msg_namelen < sizeof src || src.nl_family != AF_NETLINK
      || src.nl_pid != 0)
    return -EBADMSG;
  if (mh.msg_flags & MSG_TRUNC)
    return -EMSGSIZE;
  if (n < 16)
    return -EBADMSG;
  *got = (size_t)n;
  return 0;
#else
  return -ENOSYS;
#endif
}
int
mulib_netlink_foreach (const void *data, size_t n, mulib_netlink_visit_fn fn,
                       void *ctx)
{
  if (!data || !fn)
    return -EINVAL;
#ifdef __linux__
  const unsigned char *p = data;
  size_t off = 0;
  while (off < n)
    {
      if (n - off < sizeof (struct nlmsghdr))
        return -EBADMSG;
      struct nlmsghdr header;
      memcpy (&header, p + off, sizeof header);
      const struct nlmsghdr *h = &header;
      if (h->nlmsg_len < sizeof (*h) || h->nlmsg_len > n - off)
        return -EBADMSG;
      int r = fn (p + off, h->nlmsg_len, ctx);
      if (r)
        return r;
      size_t step = NLMSG_ALIGN (h->nlmsg_len);
      if (step > n - off)
        {
          if (h->nlmsg_len == n - off)
            return 0;
          return -EBADMSG;
        }
      off += step;
    }
  return 0;
#else
  (void)n;
  (void)ctx;
  return -ENOSYS;
#endif
}
int
mulib_netlink_transact (mulib_netlink_socket_t *s, void *msg, size_t n,
                        mulib_netlink_visit_fn visit, void *ctx)
{
  if (!s || !msg || n < 16)
    return -EINVAL;
#ifdef __linux__
  struct nlmsghdr request;
  memcpy (&request, msg, sizeof request);
  const struct nlmsghdr *req = &request;
  int dump = (req->nlmsg_flags & NLM_F_DUMP) == NLM_F_DUMP;
  int r = mulib_netlink_send (s, msg, n);
  if (r)
    return r;
  const uint32_t seq = s->seq;
  _Alignas (struct nlmsghdr) unsigned char buf[65536];
  for (unsigned round = 0; round < 1024; round++)
    {
      size_t got = 0;
      r = mulib_netlink_receive (s, buf, sizeof buf, &got);
      if (r)
        return r;
      size_t off = 0;
      while (off < got)
        {
          if (got - off < sizeof (struct nlmsghdr))
            return -EBADMSG;
          struct nlmsghdr *h = (void *)(buf + off);
          if (h->nlmsg_len < sizeof *h || h->nlmsg_len > got - off)
            return -EBADMSG;
          if (h->nlmsg_seq == seq)
            {
              if (h->nlmsg_flags & NLM_F_DUMP_INTR)
                return -EINTR;
              if (h->nlmsg_type == NLMSG_OVERRUN)
                return -ENOBUFS;
              if (h->nlmsg_type == NLMSG_NOOP)
                goto mulib_netlink_next_;
              if (h->nlmsg_type == NLMSG_ERROR)
                {
                  if (h->nlmsg_len < NLMSG_LENGTH (sizeof (struct nlmsgerr)))
                    return -EBADMSG;
                  int err;
                  memcpy (&err, NLMSG_DATA (h), sizeof err);
                  if (err)
                    return err < 0 ? err : -EBADMSG;
                  if (!dump)
                    return 0;
                }
              else if (h->nlmsg_type == NLMSG_DONE)
                {
                  if (h->nlmsg_len != NLMSG_HDRLEN
                      && h->nlmsg_len < NLMSG_LENGTH (sizeof (int)))
                    return -EBADMSG;
                  if (h->nlmsg_len >= NLMSG_LENGTH (sizeof (int)))
                    {
                      int err;
                      memcpy (&err, NLMSG_DATA (h), sizeof err);
                      if (err)
                        return err < 0 ? err : -EBADMSG;
                    }
                  return 0;
                }
              else if (visit)
                {
                  r = visit (h, h->nlmsg_len, ctx);
                  if (r)
                    return r;
                }
              if (!dump && !(req->nlmsg_flags & NLM_F_ACK))
                return 0;
            }
        mulib_netlink_next_:;
          size_t step = NLMSG_ALIGN (h->nlmsg_len);
          if (step > got - off)
            {
              if (h->nlmsg_len == got - off)
                break;
              return -EBADMSG;
            }
          off += step;
        }
    }
  return -EOVERFLOW;
#else
  (void)visit;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
