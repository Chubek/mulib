/** @file mulib-rtnl-bridge.h
 * @brief Discover Linux bridge link-kind interfaces using RTM_GETLINK linkinfo
 * filtering C11, single-header, define MULIB_IMPL_RTNL_BRIDGE exactly once.
 */
#ifndef MULIB_RTNL_BRIDGE_H
#define MULIB_RTNL_BRIDGE_H
#include "mulib-rtnl-link.h"
#include "mulib-rtnl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_rtnl_bridge_dump (mulib_netlink_visit_fn fn, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RTNL_BRIDGE) && !defined(MULIB_RTNL_BRIDGE_IMPLEMENTED)
#define MULIB_RTNL_BRIDGE_IMPLEMENTED
#include <errno.h>
struct mulib_rtnl_bridge_ctx_
{
  mulib_netlink_visit_fn fn;
  void *ctx;
};
static int
mulib_rtnl_bridge_filter_ (const void *msg, size_t n, void *p)
{
  struct mulib_rtnl_bridge_ctx_ *c = p;
  return (mulib_rtnl_link_kind (msg, n, "bridge")) && c->fn
             ? c->fn (msg, n, c->ctx)
             : 0;
}
int
mulib_rtnl_bridge_dump (mulib_netlink_visit_fn fn, void *ctx)
{
  if (!fn)
    return -EINVAL;
  struct mulib_rtnl_bridge_ctx_ c = { fn, ctx };
  return mulib_rtnl_link_dump (mulib_rtnl_bridge_filter_, &c);
}
#endif
