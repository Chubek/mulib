/** @file mulib-inet-diag.h
 * @brief Decode a Linux inet_diag_msg reply into a concise socket record
 * C11, single-header, define MULIB_IMPL_INET_DIAG exactly once.
 */
#ifndef MULIB_INET_DIAG_H
#define MULIB_INET_DIAG_H
#include "mulib-sock-diag.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_inet_diag_socket
  {
    unsigned family, state;
    uint16_t src_port, dst_port;
    uint32_t uid, inode;
    uint8_t src[16], dst[16];
  } mulib_inet_diag_socket_t;
  int mulib_inet_diag_decode (const void *nlmsg, size_t n,
                              mulib_inet_diag_socket_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INET_DIAG) && !defined(MULIB_INET_DIAG_IMPLEMENTED)
#define MULIB_INET_DIAG_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#endif
int
mulib_inet_diag_decode (const void *p, size_t n, mulib_inet_diag_socket_t *out)
{
  if (!p || !out)
    return -EINVAL;
#ifdef __linux__
  if (n < NLMSG_LENGTH (sizeof (struct inet_diag_msg)))
    return -EBADMSG;
  struct nlmsghdr header;
  memcpy (&header, p, sizeof header);
  if (header.nlmsg_len > n
      || header.nlmsg_len < NLMSG_LENGTH (sizeof (struct inet_diag_msg))
      || header.nlmsg_type != SOCK_DIAG_BY_FAMILY)
    return -EBADMSG;
  struct inet_diag_msg message;
  memcpy (&message, (const uint8_t *)p + NLMSG_HDRLEN, sizeof message);
  const struct inet_diag_msg *d = &message;
  mulib_inet_diag_socket_t s = { 0 };
  if (d->idiag_family != AF_INET && d->idiag_family != AF_INET6)
    return -EAFNOSUPPORT;
  s.family = d->idiag_family;
  s.state = d->idiag_state;
  s.src_port = ntohs (d->id.idiag_sport);
  s.dst_port = ntohs (d->id.idiag_dport);
  s.uid = d->idiag_uid;
  s.inode = d->idiag_inode;
  memcpy (s.src, d->id.idiag_src, s.family == AF_INET ? 4 : 16);
  memcpy (s.dst, d->id.idiag_dst, s.family == AF_INET ? 4 : 16);
  *out = s;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
