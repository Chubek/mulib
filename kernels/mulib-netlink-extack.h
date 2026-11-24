/** @file mulib-netlink-extack.h
 * @brief Decode NLMSG_ERROR and kernel extended acknowledgment attributes
 * C11, single-header, define MULIB_IMPL_NETLINK_EXTACK exactly once.
 */
#ifndef MULIB_NETLINK_EXTACK_H
#define MULIB_NETLINK_EXTACK_H
#include "mulib-netlink.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_netlink_error
  {
    int code;
    uint32_t offset;
    char message[256];
  } mulib_netlink_error_t;
  int mulib_netlink_extack_parse (const void *frame, size_t length,
                                  mulib_netlink_error_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETLINK_EXTACK)                                        \
    && !defined(MULIB_NETLINK_EXTACK_IMPLEMENTED)
#define MULIB_NETLINK_EXTACK_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <linux/netlink.h>
#endif
int
mulib_netlink_extack_parse (const void *p, size_t n,
                            mulib_netlink_error_t *out)
{
  if (!p || !out)
    return -EINVAL;
#ifdef __linux__
  if (n < NLMSG_LENGTH (sizeof (struct nlmsgerr)))
    return -EBADMSG;
  struct nlmsghdr header;
  memcpy (&header, p, sizeof header);
  const struct nlmsghdr *h = &header;
  if (h->nlmsg_len > n
      || h->nlmsg_len < NLMSG_LENGTH (sizeof (struct nlmsgerr))
      || h->nlmsg_type != NLMSG_ERROR)
    return -EBADMSG;
  struct nlmsgerr error;
  memcpy (&error, (const unsigned char *)p + NLMSG_HDRLEN, sizeof error);
  struct mulib_netlink_error v = { 0 };
  const struct nlmsgerr *e = &error;
  v.code = e->error;
  if (v.code > 0)
    return -EBADMSG;
  if (!(h->nlmsg_flags & NLM_F_ACK_TLVS))
    {
      *out = v;
      return 0;
    }
  size_t off = NLMSG_ALIGN (NLMSG_LENGTH (sizeof (struct nlmsgerr)));
  if (e->error && !(h->nlmsg_flags & NLM_F_CAPPED))
    {
      if (e->msg.nlmsg_len < NLMSG_HDRLEN)
        return -EBADMSG;
      size_t extra = (size_t)e->msg.nlmsg_len - NLMSG_HDRLEN;
      if (extra > h->nlmsg_len - off)
        return -EBADMSG;
      extra = (extra + 3u) & ~(size_t)3;
      if (extra > h->nlmsg_len - off)
        return -EBADMSG;
      off += extra;
    }
  while (off < h->nlmsg_len)
    {
      if (h->nlmsg_len - off < sizeof (struct nlattr))
        return -EBADMSG;
      struct nlattr attr;
      memcpy (&attr, (const unsigned char *)p + off, sizeof attr);
      const struct nlattr *a = &attr;
      if (a->nla_len < sizeof *a || a->nla_len > h->nlmsg_len - off)
        return -EBADMSG;
      uint16_t type = a->nla_type & NLA_TYPE_MASK;
      const unsigned char *b = (const unsigned char *)p + off + sizeof *a;
      size_t sz = a->nla_len - sizeof *a;
      if (type == NLMSGERR_ATTR_MSG)
        {
          if (!sz || b[sz - 1] != 0)
            return -EBADMSG;
          size_t copy = sz - 1 < sizeof (v.message) - 1
                            ? sz - 1
                            : sizeof (v.message) - 1;
          memcpy (v.message, b, copy);
          v.message[copy] = 0;
        }
      if (type == NLMSGERR_ATTR_OFFS)
        {
          if (sz != 4)
            return -EBADMSG;
          memcpy (&v.offset, b, 4);
        }
      size_t step = NLA_ALIGN (a->nla_len);
      if (step > h->nlmsg_len - off)
        {
          if (a->nla_len != h->nlmsg_len - off)
            return -EBADMSG;
          off = h->nlmsg_len;
        }
      else
        off += step;
    }
  *out = v;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
