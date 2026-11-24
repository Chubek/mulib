/** @file mulib-nfnetlink.h
 * @brief Open Linux NETLINK_NETFILTER and send bounded netfilter messages
 * C11, single-header, define MULIB_IMPL_NFNETLINK exactly once.
 */
#ifndef MULIB_NFNETLINK_H
#define MULIB_NFNETLINK_H
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_nfnetlink_open (mulib_netlink_socket_t *sock);
  int mulib_nfnetlink_send (mulib_netlink_socket_t *sock, uint16_t type,
                            uint8_t family, uint8_t version,
                            const void *payload, size_t length,
                            mulib_netlink_visit_fn cb, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NFNETLINK) && !defined(MULIB_NFNETLINK_IMPLEMENTED)
#define MULIB_NFNETLINK_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <linux/netfilter/nfnetlink.h>
#endif
int
mulib_nfnetlink_open (mulib_netlink_socket_t *s)
{
#ifdef __linux__
  return mulib_netlink_open (s, NETLINK_NETFILTER, 0);
#else
  return -ENOSYS;
#endif
}
int
mulib_nfnetlink_send (mulib_netlink_socket_t *s, uint16_t type, uint8_t family,
                      uint8_t ver, const void *p, size_t n,
                      mulib_netlink_visit_fn cb, void *ctx)
{
  if (!s || (!p && n) || n > 16384)
    return -EINVAL;
#ifdef __linux__
  _Alignas(struct nlmsghdr) unsigned char buf[16384 + 64] = { 0 };
  struct nlmsghdr *h = (void *)buf;
  struct nfgenmsg *g = (void *)NLMSG_DATA (h);
  g->nfgen_family = family;
  g->version = ver;
  g->res_id = 0;
  if (n)
    memcpy ((unsigned char *)g + sizeof *g, p, n);
  h->nlmsg_type = type;
  h->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
  h->nlmsg_len = NLMSG_LENGTH (sizeof *g + n);
  return mulib_netlink_transact (s, h, h->nlmsg_len, cb, ctx);
#else
  (void)type;
  (void)family;
  (void)ver;
  (void)cb;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
