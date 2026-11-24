/** @file mulib-conntrack.h
 * @brief Dump conntrack table entries through netfilter conntrack Netlink
 * subsystem C11, single-header, define MULIB_IMPL_CONNTRACK exactly once.
 */
#ifndef MULIB_CONNTRACK_H
#define MULIB_CONNTRACK_H
#include "mulib-nfnetlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_conntrack_dump (unsigned family, mulib_netlink_visit_fn fn,
                            void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONNTRACK) && !defined(MULIB_CONNTRACK_IMPLEMENTED)
#define MULIB_CONNTRACK_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#endif
int
mulib_conntrack_dump (unsigned family, mulib_netlink_visit_fn fn, void *ctx)
{
  if (family != 0 && family != 4 && family != 6)
    return -EINVAL;
#ifdef __linux__
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  int r = mulib_nfnetlink_open (&s);
  if (r)
    return r;
  struct
  {
    struct nlmsghdr nl;
    struct nfgenmsg g;
  } req = { 0 };
  req.nl.nlmsg_len = sizeof req;
  req.nl.nlmsg_type = (NFNL_SUBSYS_CTNETLINK << 8) | IPCTNL_MSG_CT_GET;
  req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  req.g.nfgen_family = (uint8_t)(family == 4   ? AF_INET
                                 : family == 6 ? AF_INET6
                                               : AF_UNSPEC);
  req.g.version = NFNETLINK_V0;
  r = mulib_netlink_transact (&s, &req, sizeof req, fn, ctx);
  mulib_netlink_close (&s);
  return r;
#else
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
