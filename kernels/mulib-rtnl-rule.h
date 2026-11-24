/** @file mulib-rtnl-rule.h
 * @brief Dump kernel rule objects via RTM_GETRULE
 * C11, single-header, define MULIB_IMPL_RTNL_RULE exactly once.
 */
#ifndef MULIB_RTNL_RULE_H
#define MULIB_RTNL_RULE_H
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_rule_dump (unsigned family, mulib_netlink_visit_fn fn,
                            void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_RULE) && !defined(MULIB_RTNL_RULE_IMPLEMENTED)
#define MULIB_RTNL_RULE_IMPLEMENTED
#include <errno.h>
#ifdef __linux__
#include <linux/fib_rules.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#endif
int
mulib_rtnl_rule_dump (unsigned family, mulib_netlink_visit_fn fn, void *ctx)
{
  if (family != 0 && family != 4 && family != 6)
    return -EINVAL;
#ifdef __linux__
  struct fib_rule_hdr m = { 0 };
  m.family = (uint8_t)(family == 4   ? AF_INET
                       : family == 6 ? AF_INET6
                                     : AF_UNSPEC);
  return mulib_rtnl_dump (RTM_GETRULE, &m, sizeof m, fn, ctx);
#else
  (void)fn;
  (void)ctx;
  return -ENOSYS;
#endif
}
#endif
