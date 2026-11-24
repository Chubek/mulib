/** @file mulib-sock-diag.h
 * @brief Issue a bounded NETLINK_SOCK_DIAG inet socket dump request
 * C11, single-header, define MULIB_IMPL_SOCK_DIAG exactly once.
 */
#ifndef MULIB_SOCK_DIAG_H
#define MULIB_SOCK_DIAG_H
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_sock_diag_dump (unsigned family, unsigned protocol,
                            mulib_netlink_visit_fn fn, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SOCK_DIAG) && !defined(MULIB_SOCK_DIAG_IMPLEMENTED)
#define MULIB_SOCK_DIAG_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <netinet/in.h>
#endif
int
mulib_sock_diag_dump (unsigned family, unsigned proto,
                      mulib_netlink_visit_fn fn, void *ctx)
{
  if ((family != 4 && family != 6) || (proto != 6 && proto != 17))
    return -EINVAL;
#ifdef __linux__
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  int r = mulib_netlink_open (&s, NETLINK_SOCK_DIAG, 0);
  if (r)
    return r;
  struct
  {
    struct nlmsghdr h;
    struct inet_diag_req_v2 q;
  } pkt = { 0 };
  pkt.h.nlmsg_len = sizeof pkt;
  pkt.h.nlmsg_type = SOCK_DIAG_BY_FAMILY;
  pkt.h.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  pkt.q.sdiag_family = (uint8_t)(family == 4 ? AF_INET : AF_INET6);
  pkt.q.sdiag_protocol = (uint8_t)proto;
  pkt.q.idiag_states = 0xffffffffu;
  r = mulib_netlink_transact (&s, &pkt, sizeof pkt, fn, ctx);
  mulib_netlink_close (&s);
  return r;
#else
  return -ENOSYS;
#endif
}
#endif
