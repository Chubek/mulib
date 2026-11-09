/** @file mulib-nftables.h
 * @brief Dump kernel nftables tables through NFNETLINK_SUBSYS_NFTABLES
 * C11, single-header, define MULIB_IMPL_NFTABLES exactly once.
 */
#ifndef MULIB_NFTABLES_H
#define MULIB_NFTABLES_H
#include "mulib-nfnetlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_nftables_tables (mulib_netlink_visit_fn fn, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NFTABLES) && !defined(MULIB_NFTABLES_IMPLEMENTED)
#define MULIB_NFTABLES_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/netfilter/nf_tables.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netlink.h>
#endif
int
mulib_nftables_tables (mulib_netlink_visit_fn fn, void *ctx)
{
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
  req.nl.nlmsg_type = (NFNL_SUBSYS_NFTABLES << 8) | NFT_MSG_GETTABLE;
  req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  /* AF_UNSPEC == NFPROTO_UNSPEC on Linux. Avoid linux/netfilter.h, whose
   * linux/in.h declarations conflict with libc after unrelated UAPI headers
   * have frozen linux/libc-compat.h's feature decisions. */
  req.g.nfgen_family = 0;
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
