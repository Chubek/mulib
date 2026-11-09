/** @file mulib-rtnl.h
 * @brief NETLINK_ROUTE request builder and bounded synchronous kernel dumps
 * C11, single-header, define MULIB_IMPL_RTNL exactly once.
 */
#ifndef MULIB_RTNL_H
#define MULIB_RTNL_H
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_request (uint16_t msg_type, uint16_t flags, const void *body,
                          size_t body_n, const void *attrs, size_t attrs_n,
                          mulib_netlink_visit_fn fn, void *ctx);
  int mulib_rtnl_dump (uint16_t get_type, const void *body, size_t body_n,
                       mulib_netlink_visit_fn fn, void *ctx);
  int mulib_rtnl_link_kind (const void *nlmsg, size_t size, const char *kind);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL) && !defined(MULIB_RTNL_IMPLEMENTED)
#define MULIB_RTNL_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#endif
int
mulib_rtnl_request (uint16_t type, uint16_t flags, const void *body, size_t bn,
                    const void *attrs, size_t an, mulib_netlink_visit_fn fn,
                    void *ctx)
{
  if ((!body && bn) || (!attrs && an) || bn > 16384 || an > 16384)
    return -EINVAL;
#ifdef __linux__
  _Alignas(struct nlmsghdr) unsigned char buf[32768] = { 0 };
  size_t base = NLMSG_ALIGN (NLMSG_LENGTH (bn));
  if (base + an > sizeof buf)
    return -E2BIG;
  struct nlmsghdr *h = (void *)buf;
  h->nlmsg_type = type;
  h->nlmsg_flags = (uint16_t)(flags | NLM_F_REQUEST);
  h->nlmsg_len = base + an;
  if (bn)
    memcpy (NLMSG_DATA (h), body, bn);
  if (an)
    memcpy (buf + base, attrs, an);
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  int r = mulib_netlink_open (&s, NETLINK_ROUTE, 0);
  if (r)
    return r;
  r = mulib_netlink_transact (&s, h, h->nlmsg_len, fn, ctx);
  mulib_netlink_close (&s);
  return r;
#else
  (void)type;
  (void)flags;
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
int
mulib_rtnl_dump (uint16_t typ, const void *body, size_t n,
                 mulib_netlink_visit_fn fn, void *ctx)
{
#ifdef __linux__
  return mulib_rtnl_request (typ, NLM_F_DUMP, body, n, NULL, 0, fn, ctx);
#else
  (void)typ;
  (void)body;
  (void)n;
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
int
mulib_rtnl_link_kind (const void *p, size_t n, const char *kind)
{
  if (!p || !kind)
    return 0;
#ifdef __linux__
  if (n < NLMSG_LENGTH (sizeof (struct ifinfomsg)))
    return 0;
  struct nlmsghdr header;
  memcpy (&header, p, sizeof header);
  const struct nlmsghdr *h = &header;
  if (h->nlmsg_len > n
      || h->nlmsg_len < NLMSG_LENGTH (sizeof (struct ifinfomsg))
      || h->nlmsg_type != RTM_NEWLINK)
    return 0;
  n = h->nlmsg_len;
  size_t off = NLMSG_ALIGN (NLMSG_LENGTH (sizeof (struct ifinfomsg)));
  const unsigned char *base = p;
  while (off + sizeof (struct rtattr) <= n)
    {
      struct rtattr attr;
      memcpy (&attr, base + off, sizeof attr);
      const struct rtattr *a = &attr;
      if (a->rta_len < sizeof (*a) || a->rta_len > n - off)
        return 0;
      if ((a->rta_type & NLA_TYPE_MASK) == IFLA_LINKINFO)
        {
          const unsigned char *sub = base + off + RTA_LENGTH (0);
          size_t left = a->rta_len - RTA_LENGTH (0);
          while (left >= sizeof (struct rtattr))
            {
              struct rtattr child;
              memcpy (&child, sub, sizeof child);
              const struct rtattr *b = &child;
              if (b->rta_len < sizeof (*b) || b->rta_len > left)
                return 0;
              if ((b->rta_type & NLA_TYPE_MASK) == IFLA_INFO_KIND)
                {
                  size_t sz = b->rta_len - RTA_LENGTH (0);
                  return strlen (kind) + 1 == sz
                         && !memcmp (sub + RTA_LENGTH (0), kind, sz);
                }
              size_t st = RTA_ALIGN (b->rta_len);
              if (st > left)
                break;
              sub += st;
              left -= st;
            }
        }
      size_t st = RTA_ALIGN (a->rta_len);
      if (st > n - off)
        break;
      off += st;
    }
  return 0;
#else
  (void)n;
  return 0;
#endif
}
#endif
