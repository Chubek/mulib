/* Deterministic Netlink replies, without touching kernel network state. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

static uint8_t frames[128];
static size_t frame_size;
static unsigned receives;
static ssize_t
mock_sendto (int fd, const void *p, size_t n, int flags,
             const struct sockaddr *dst, socklen_t length)
{
  (void)fd;
  (void)flags;
  (void)dst;
  (void)length;
  struct nlmsghdr h;
  memcpy (&h, p, sizeof h);
  /* Every synthesized reply belongs to this request. */
  for (size_t off = 0; off < frame_size;)
    {
      struct nlmsghdr r;
      memcpy (&r, frames + off, sizeof r);
      r.nlmsg_seq = h.nlmsg_seq;
      memcpy (frames + off, &r, sizeof r);
      off += NLMSG_ALIGN (r.nlmsg_len);
    }
  return (ssize_t)n;
}
static ssize_t
mock_recvmsg (int fd, struct msghdr *m, int flags)
{
  (void)fd;
  (void)flags;
  assert (++receives == 1);
  assert (frame_size <= sizeof frames);
  assert (m->msg_iov[0].iov_len >= frame_size);
  memcpy (m->msg_iov[0].iov_base, frames, frame_size);
  struct sockaddr_nl src = { 0 };
  src.nl_family = AF_NETLINK;
  memcpy (m->msg_name, &src, sizeof src);
  m->msg_namelen = sizeof src;
  return (ssize_t)frame_size;
}
#define sendto mock_sendto
#define recvmsg mock_recvmsg
#define MULIB_IMPL_NETLINK
#include "mulib-netlink.h"
#undef sendto
#undef recvmsg

static int visits;
static int
visit (const void *p, size_t n, void *ctx)
{
  (void)p;
  (void)n;
  (void)ctx;
  ++visits;
  return 0;
}
static void
append (uint16_t type, uint16_t flags, const void *p, size_t n)
{
  assert (n <= sizeof frames - NLMSG_HDRLEN);
  assert (frame_size <= sizeof frames - NLMSG_ALIGN (NLMSG_LENGTH (n)));
  struct nlmsghdr h = { 0 };
  h.nlmsg_len = (uint32_t)NLMSG_LENGTH (n);
  h.nlmsg_type = type;
  h.nlmsg_flags = flags;
  memcpy (frames + frame_size, &h, sizeof h);
  if (n)
    memcpy (frames + frame_size + NLMSG_HDRLEN, p, n);
  frame_size += NLMSG_ALIGN (h.nlmsg_len);
}
static int
transact (uint16_t flags)
{
  mulib_netlink_socket_t socket = { 123, 1, UINT32_MAX, NETLINK_ROUTE };
  struct nlmsghdr req = { 0 };
  req.nlmsg_len = sizeof req;
  req.nlmsg_flags = NLM_F_REQUEST | flags;
  receives = 0;
  visits = 0;
  int r = mulib_netlink_transact (&socket, &req, sizeof req, visit, NULL);
  assert (req.nlmsg_seq == 1);
  return r;
}
int
main (void)
{
  append (NLMSG_NOOP, 0, NULL, 0);
  append (RTM_NEWLINK, 0, NULL, 0);
  assert (!transact (0) && visits == 1);
  frame_size = 0;
  append (NLMSG_DONE, NLM_F_DUMP_INTR, NULL, 0);
  assert (transact (NLM_F_DUMP) == -EINTR);
  frame_size = 0;
  append (NLMSG_OVERRUN, 0, NULL, 0);
  assert (transact (NLM_F_DUMP) == -ENOBUFS);
  frame_size = 0;
  struct nlmsgerr error = { 0 };
  error.error = EPERM;
  append (NLMSG_ERROR, 0, &error, sizeof error);
  assert (transact (NLM_F_ACK) == -EBADMSG);
  frame_size = 0;
  error.error = 0;
  append (NLMSG_ERROR, 0, &error, sizeof error);
  append (RTM_NEWLINK, NLM_F_MULTI, NULL, 0);
  int status = 0;
  append (NLMSG_DONE, 0, &status, sizeof status);
  assert (!transact (NLM_F_DUMP | NLM_F_ACK) && visits == 1);
  return 0;
}
