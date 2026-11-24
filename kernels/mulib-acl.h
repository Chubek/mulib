/** @file mulib-acl.h
 * @brief First-match IPv4/IPv6 CIDR access control over borrowed rules.
 * Addresses use numeric network significance (127.0.0.1 = 0x7f000001).
 * Define MULIB_IMPL_ACL, INET6, INET4 and PARSEC. Invalid rules fail closed.
 * Evaluate the actual peer address, never untrusted forwarding headers. */
#ifndef MULIB_ACL_H
#define MULIB_ACL_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-inet6.h"
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
  /** Validate every rule before evaluation, including unreachable rules. */
  int mulib_acl_validate (const mulib_acl_rule_t *rules, size_t count);
  /** First-match check with optional matched index (SIZE_MAX on fallback or
   * invalid policy). Denial and invalid policy both return zero. */
  int mulib_acl_check_ex (const mulib_acl_rule_t *rules, size_t count,
                          uint32_t address, int default_allow, size_t *matched);
  /** Strict address/prefix CIDR parser; host bits are normalized. No whitespace,
   * signs, leading zero prefix lengths or implicit prefixes. Output unchanged
   * on failure. */
  int mulib_acl_rule_parse (const char *cidr, int allow, mulib_acl_rule_t *out);
  typedef struct mulib_acl6_rule {
    mulib_inet6_t network;
    unsigned prefix;
    int allow;
  } mulib_acl6_rule_t;
  /** IPv6 CIDR parser, same policy as the IPv4 parser; no zones or brackets. */
  int mulib_acl6_rule_parse (const char *cidr, int allow, mulib_acl6_rule_t *out);
  /** Validate a borrowed IPv6 rule array. */
  int mulib_acl6_validate (const mulib_acl6_rule_t *rules, size_t count);
  /** First-match IPv6 policy. IPv4-mapped addresses are not implicitly IPv4;
   * choose the correct family explicitly at the transport boundary. */
  int mulib_acl6_check (const mulib_acl6_rule_t *rules, size_t count,
                        const mulib_inet6_t *address, int default_allow,
                        size_t *matched);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ACL) && !defined(MULIB_ACL_IMPLEMENTED)
#define MULIB_ACL_IMPLEMENTED
#include <string.h>
int
mulib_acl_validate (const mulib_acl_rule_t *r, size_t n)
{
  if (!r && n) return 0;
  for (size_t i=0; i<n; ++i)
    if (r[i].prefix>32 || (r[i].allow!=0 && r[i].allow!=1)) return 0;
  return 1;
}
int
mulib_acl_check (const mulib_acl_rule_t *r, size_t n, uint32_t ip, int fallback)
{
  return mulib_acl_check_ex(r,n,ip,fallback,NULL);
}
int
mulib_acl_check_ex (const mulib_acl_rule_t *r, size_t n, uint32_t ip,
                     int fallback, size_t *matched)
{
  if (matched) *matched=SIZE_MAX;
  if (!mulib_acl_validate(r,n) || (fallback != 0 && fallback != 1))
    return 0;
  for (size_t i = 0; i < n; ++i)
    {
      uint32_t mask = r[i].prefix ? UINT32_MAX << (32 - r[i].prefix) : 0;
      if ((ip & mask) == (r[i].network & mask))
        {
          if (matched) *matched=i;
          return r[i].allow;
        }
    }
  return fallback;
}
static int
mulib_acl_prefix_ (const char *s, size_t maximum, unsigned *prefix, size_t *length)
{
  size_t n=0, slash=0;
  unsigned value=0;
  if (!s) return 0;
  while (n<=maximum && s[n]) ++n;
  if (!n || n>maximum) return 0;
  while (slash<n && s[slash]!='/') ++slash;
  if (!slash || slash+1>=n || (s[slash+1]=='0' && slash+2<n)) return 0;
  for (size_t i=slash+1; i<n; ++i) {
    if (s[i]<'0' || s[i]>'9' || value>128) return 0;
    value=value*10+(unsigned)(s[i]-'0');
  }
  if (value>128) return 0;
  *prefix=value; *length=slash;
  return 1;
}
int
mulib_acl_rule_parse (const char *s, int allow, mulib_acl_rule_t *out)
{
  mulib_acl_rule_t r;
  mulib_inet4_t a;
  size_t length;
  if (!out || (allow!=0 && allow!=1)
      || !mulib_acl_prefix_(s,18,&r.prefix,&length) || r.prefix>32
      || !mulib_inet4_parse(s,length,&a)) return 0;
  r.network=(uint32_t)a.bytes[0]<<24 | (uint32_t)a.bytes[1]<<16
            | (uint32_t)a.bytes[2]<<8 | a.bytes[3];
  r.network &= r.prefix ? UINT32_MAX<<(32-r.prefix) : 0;
  r.allow=allow;
  *out=r;
  return 1;
}
int
mulib_acl6_rule_parse (const char *s, int allow, mulib_acl6_rule_t *out)
{
  mulib_acl6_rule_t r;
  mulib_inet6_t a;
  size_t length;
  if (!out || (allow!=0 && allow!=1)
      || !mulib_acl_prefix_(s,49,&r.prefix,&length)
      || !mulib_inet6_parse(s,length,&a)
      || !mulib_inet6_network(&a,r.prefix,&r.network)) return 0;
  r.allow=allow;
  *out=r;
  return 1;
}
int
mulib_acl6_validate (const mulib_acl6_rule_t *r, size_t n)
{
  if (!r && n) return 0;
  for (size_t i=0; i<n; ++i)
    if (r[i].prefix>128 || (r[i].allow!=0 && r[i].allow!=1)) return 0;
  return 1;
}
int
mulib_acl6_check (const mulib_acl6_rule_t *r, size_t n, const mulib_inet6_t *ip,
                   int fallback, size_t *matched)
{
  if (matched) *matched=SIZE_MAX;
  if (!ip || !mulib_acl6_validate(r,n) || (fallback!=0 && fallback!=1)) return 0;
  for (size_t i=0; i<n; ++i)
    if (mulib_inet6_contains(&r[i].network,r[i].prefix,ip)) {
      if (matched) *matched=i;
      return r[i].allow;
    }
  return fallback;
}
#endif
