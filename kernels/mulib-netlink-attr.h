/** @file mulib-netlink-attr.h
 * @brief Bounded aligned RTATTR and NLA encode/decode helpers
 * C11, single-header, define MULIB_IMPL_NETLINK_ATTR exactly once.
 */
#ifndef MULIB_NETLINK_ATTR_H
#define MULIB_NETLINK_ATTR_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_netlink_attr
  {
    uint16_t type;
    const void *value;
    size_t length;
  } mulib_netlink_attr_t;
  typedef int (*mulib_netlink_attr_fn) (const mulib_netlink_attr_t *, void *);
  int mulib_netlink_attr_append (void *buf, size_t cap, size_t *used,
                                 uint16_t type, const void *data,
                                 size_t length);
  int mulib_netlink_attr_foreach (const void *buf, size_t n,
                                  mulib_netlink_attr_fn fn, void *ctx);
  int mulib_netlink_attr_u32 (const mulib_netlink_attr_t *a, uint32_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETLINK_ATTR)                                          \
    && !defined(MULIB_NETLINK_ATTR_IMPLEMENTED)
#define MULIB_NETLINK_ATTR_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif
int
mulib_netlink_attr_append (void *p, size_t cap, size_t *used, uint16_t type,
                           const void *v, size_t n)
{
  if (!p || !used || (!v && n) || n > 65531 || *used > cap)
    return -EINVAL;
#ifdef __linux__
  size_t total = RTA_LENGTH (n), step = RTA_ALIGN (total);
  if (step > cap - *used)
    return -ENOSPC;
  struct rtattr h = { (unsigned short)total, type };
  unsigned char *b = (unsigned char *)p + *used;
  memcpy (b, &h, sizeof h);
  if (n)
    memcpy (b + RTA_LENGTH (0), v, n);
  if (step > total)
    memset (b + total, 0, step - total);
  *used += step;
  return 0;
#else
  (void)type;
  return -ENOSYS;
#endif
}
int
mulib_netlink_attr_foreach (const void *p, size_t n, mulib_netlink_attr_fn fn,
                            void *ctx)
{
  if (!p || !fn)
    return -EINVAL;
#ifdef __linux__
  const unsigned char *b = p;
  size_t off = 0;
  while (off < n)
    {
      if (n - off < sizeof (struct rtattr))
        return -EBADMSG;
      struct rtattr header;
      memcpy (&header, b + off, sizeof header);
      const struct rtattr *a = &header;
      if (a->rta_len < sizeof *a || a->rta_len > n - off)
        return -EBADMSG;
      mulib_netlink_attr_t v = { a->rta_type, b + off + RTA_LENGTH (0),
                                 a->rta_len - RTA_LENGTH (0) };
      int rc = fn (&v, ctx);
      if (rc)
        return rc;
      size_t step = RTA_ALIGN (a->rta_len);
      if (step > n - off)
        {
          if (a->rta_len == n - off)
            return 0;
          return -EBADMSG;
        }
      off += step;
    }
  return 0;
#else
  (void)n;
  (void)ctx;
  return -ENOSYS;
#endif
}
int
mulib_netlink_attr_u32 (const mulib_netlink_attr_t *a, uint32_t *out)
{
  if (!a || !a->value || !out || a->length != 4)
    return -EINVAL;
  memcpy (out, a->value, 4);
  return 0;
}
#endif
