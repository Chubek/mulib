/** @file mulib-net-reconcile.h
 * @brief Read-only desired-versus-observed interface activation comparison
 * C11, single-header, define MULIB_IMPL_NET_RECONCILE exactly once.
 */
#ifndef MULIB_NET_RECONCILE_H
#define MULIB_NET_RECONCILE_H
#include "mulib-net-profile.h"
#include "mulib-net-state.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_net_reconcile_result
  {
    int interface_present;
    int oper_up;
    int differs;
  } mulib_net_reconcile_result_t;
  int mulib_net_reconcile_check (const mulib_net_profile_t *desired,
                                 mulib_net_reconcile_result_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NET_RECONCILE)                                         \
    && !defined(MULIB_NET_RECONCILE_IMPLEMENTED)
#define MULIB_NET_RECONCILE_IMPLEMENTED
#include <errno.h>
int
mulib_net_reconcile_check (const mulib_net_profile_t *profile,
                           mulib_net_reconcile_result_t *out)
{
  if (!profile || !out || !profile->interface[0]
      || !memchr (profile->interface, 0, sizeof profile->interface)
      || profile->enabled > 1)
    return -EINVAL;
  mulib_net_state_t state;
  int r = mulib_net_state_read (profile->interface, &state);
  if (r && r != -ENODEV && r != -ENOENT)
    return r;
  mulib_net_reconcile_result_t v = { r == 0, r == 0 ? state.oper_up : 0, 0 };
  v.differs = (!v.interface_present) || ((int)profile->enabled != v.oper_up);
  *out = v;
  return 0;
}
#endif
