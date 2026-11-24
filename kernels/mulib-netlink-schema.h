/** @file mulib-netlink-schema.h
 * @brief Typed fixed-width policy checks for a collection of parsed Netlink
 * attributes C11, single-header, define MULIB_IMPL_NETLINK_SCHEMA exactly
 * once.
 */
#ifndef MULIB_NETLINK_SCHEMA_H
#define MULIB_NETLINK_SCHEMA_H
#include "mulib-netlink-attr.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_netlink_policy
  {
    uint16_t type;
    size_t minimum, maximum;
    int required;
  } mulib_netlink_policy_t;
  int mulib_netlink_schema_check (const void *attrs, size_t n,
                                  const mulib_netlink_policy_t *rules,
                                  size_t count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETLINK_SCHEMA)                                        \
    && !defined(MULIB_NETLINK_SCHEMA_IMPLEMENTED)
#define MULIB_NETLINK_SCHEMA_IMPLEMENTED
#include <errno.h>
#include <string.h>
struct mulib_schema_ctx_
{
  const mulib_netlink_policy_t *rules;
  size_t count;
  unsigned char seen[256];
};
static int
mulib_schema_visit_ (const mulib_netlink_attr_t *a, void *p)
{
  struct mulib_schema_ctx_ *c = p;
  for (size_t i = 0; i < c->count; i++)
    if (c->rules[i].type == (a->type & 0x3fffu))
      {
        if (a->length < c->rules[i].minimum || a->length > c->rules[i].maximum)
          return -EBADMSG;
        c->seen[i] = 1;
        break;
      }
  return 0;
}
int
mulib_netlink_schema_check (const void *p, size_t n,
                            const mulib_netlink_policy_t *r, size_t cnt)
{
  if ((!p && n) || (!r && cnt) || cnt > 256)
    return -EINVAL;
  for (size_t i = 0; i < cnt; i++)
    {
      if (r[i].minimum > r[i].maximum || r[i].type > 0x3fff)
        return -EINVAL;
      for (size_t j = 0; j < i; j++)
        if (r[j].type == r[i].type)
          return -EINVAL;
    }
  struct mulib_schema_ctx_ c = { r, cnt, { 0 } };
  int rc = n ? mulib_netlink_attr_foreach (p, n, mulib_schema_visit_, &c) : 0;
  if (rc)
    return rc;
  for (size_t i = 0; i < cnt; i++)
    if (r[i].required && !c.seen[i])
      return -ENOENT;
  return 0;
}
#endif
