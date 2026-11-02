/** @file mulib-acl.h
 * @brief First-match IPv4 CIDR access control over borrowed rules.
 * Addresses use numeric network significance (127.0.0.1 = 0x7f000001).
 * Define MULIB_IMPL_ACL. Invalid rules fail closed. */
#ifndef MULIB_ACL_H
#define MULIB_ACL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_acl_rule
  {
    uint32_t network;
    unsigned prefix;
    int allow;
  } mulib_acl_rule_t;
  int mulib_acl_check (const mulib_acl_rule_t *rules, size_t count,
                       uint32_t address, int default_allow);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ACL) && !defined(MULIB_ACL_IMPLEMENTED)
#define MULIB_ACL_IMPLEMENTED
int
mulib_acl_check (const mulib_acl_rule_t *r, size_t n, uint32_t ip, int fallback)
{
  if ((!r && n) || (fallback != 0 && fallback != 1))
    return 0;
  for (size_t i = 0; i < n; ++i)
    if (r[i].prefix > 32 || (r[i].allow != 0 && r[i].allow != 1))
      return 0;
  for (size_t i = 0; i < n; ++i)
    {
      uint32_t mask = r[i].prefix ? UINT32_MAX << (32 - r[i].prefix) : 0;
      if ((ip & mask) == (r[i].network & mask))
        return r[i].allow;
    }
  return fallback;
}
#endif
