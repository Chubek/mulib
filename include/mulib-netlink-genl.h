/** @file mulib-netlink-genl.h
 * @brief Resolve Generic Netlink family IDs through the kernel control family
 * C11, single-header, define MULIB_IMPL_NETLINK_GENL exactly once.
 */
#ifndef MULIB_NETLINK_GENL_H
#define MULIB_NETLINK_GENL_H
#include "mulib-netlink-attr.h"
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_netlink_genl_resolve (mulib_netlink_socket_t *s,
                                  const char *family, uint16_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETLINK_GENL)                                          \
    && !defined(MULIB_NETLINK_GENL_IMPLEMENTED)
#define MULIB_NETLINK_GENL_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <linux/genetlink.h>
#endif
struct mulib_genl_ctx_
{
  uint16_t *out;
  int seen;
};
static int
mulib_genl_visit_ (const void *m, size_t n, void *arg)
{
#ifdef __linux__
  struct mulib_genl_ctx_ *ctx = arg;
  if (n < NLMSG_LENGTH (sizeof (struct genlmsghdr)))
    return -EBADMSG;
  const unsigned char *data
      = (const unsigned char *)m + NLMSG_HDRLEN + GENL_HDRLEN;
  size_t left = n - NLMSG_LENGTH (GENL_HDRLEN);
  while (left)
    {
      if (left < sizeof (struct nlattr))
        return -EBADMSG;
      struct nlattr attr;
      memcpy (&attr, data, sizeof attr);
      const struct nlattr *a = &attr;
      if (a->nla_len < sizeof *a || a->nla_len > left)
        return -EBADMSG;
      if ((a->nla_type & NLA_TYPE_MASK) == CTRL_ATTR_FAMILY_ID)
        {
          if (a->nla_len != sizeof (*a) + 2 || ctx->seen)
            return -EBADMSG;
          memcpy (ctx->out, data + sizeof *a, 2);
          ctx->seen = 1;
        }
      size_t step = NLA_ALIGN (a->nla_len);
      if (step > left)
        return a->nla_len == left ? 0 : -EBADMSG;
      data += step;
      left -= step;
    }
  return 0;
#else
  (void)m;
  (void)n;
  (void)arg;
  return -ENOSYS;
#endif
}
int
mulib_netlink_genl_resolve (mulib_netlink_socket_t *s, const char *family,
                            uint16_t *out)
{
  if (!s || !family || !out || !family[0] || strlen (family) >= 16)
    return -EINVAL;
#ifdef __linux__
  _Alignas(struct nlmsghdr) unsigned char buf[512] = { 0 };
  struct nlmsghdr *h = (void *)buf;
  struct genlmsghdr *g = (void *)NLMSG_DATA (h);
  g->cmd = CTRL_CMD_GETFAMILY;
  g->version = 1;
  size_t used = NLMSG_LENGTH (GENL_HDRLEN);
  int r = mulib_netlink_attr_append (buf, sizeof buf, &used,
                                     CTRL_ATTR_FAMILY_NAME, family,
                                     strlen (family) + 1);
  if (r)
    return r;
  h->nlmsg_type = GENL_ID_CTRL;
  h->nlmsg_flags = NLM_F_REQUEST;
  h->nlmsg_len = used;
  uint16_t id = 0;
  struct mulib_genl_ctx_ ctx = { &id, 0 };
  r = mulib_netlink_transact (s, h, used, mulib_genl_visit_, &ctx);
  if (r)
    return r;
  if (!ctx.seen || id < NLMSG_MIN_TYPE)
    return -ENOENT;
  *out = id;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
