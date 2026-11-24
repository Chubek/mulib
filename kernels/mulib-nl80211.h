/** @file mulib-nl80211.h
 * @brief Generic Netlink nl80211 request helper for wireless devices
 * C11, single-header, define MULIB_IMPL_NL80211 exactly once.
 */
#ifndef MULIB_NL80211_H
#define MULIB_NL80211_H
#include "mulib-netlink-attr.h"
#include "mulib-netlink-genl.h"
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_nl80211_command (uint8_t command, unsigned ifindex, unsigned flags,
                             mulib_netlink_visit_fn visit, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NL80211) && !defined(MULIB_NL80211_IMPLEMENTED)
#define MULIB_NL80211_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#endif
int
mulib_nl80211_command (uint8_t cmd, unsigned idx, unsigned flags,
                       mulib_netlink_visit_fn fn, void *ctx)
{
#ifdef __linux__
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  int r = mulib_netlink_open (&s, NETLINK_GENERIC, 0);
  if (r)
    return r;
  uint16_t family = 0;
  r = mulib_netlink_genl_resolve (&s, "nl80211", &family);
  if (r)
    {
      mulib_netlink_close (&s);
      return r;
    }
  _Alignas(struct nlmsghdr) unsigned char pkt[256] = { 0 };
  struct nlmsghdr *h = (void *)pkt;
  struct genlmsghdr *g = (void *)NLMSG_DATA (h);
  g->cmd = cmd;
  g->version = 0;
  size_t used = NLMSG_LENGTH (GENL_HDRLEN);
  if (idx)
    {
      r = mulib_netlink_attr_append (pkt, sizeof pkt, &used,
                                     NL80211_ATTR_IFINDEX, &idx, 4);
      if (r)
        {
          mulib_netlink_close (&s);
          return r;
        }
    }
  h->nlmsg_len = used;
  h->nlmsg_type = family;
  h->nlmsg_flags = (uint16_t)(NLM_F_REQUEST | flags);
  r = mulib_netlink_transact (&s, pkt, used, fn, ctx);
  mulib_netlink_close (&s);
  return r;
#else
  (void)cmd;
  (void)idx;
  (void)flags;
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
