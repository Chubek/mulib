/** @file mulib-browser-permissions.h
 * @brief Explicit bitmask permissions for browser capabilities.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_PERMISSIONS in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_PERMISSIONS_H
#define MULIB_BROWSER_PERMISSIONS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_browser_permission
  {
    MULIB_BROWSER_ALLOW_NETWORK = 1u,
    MULIB_BROWSER_ALLOW_STORAGE = 2u,
    MULIB_BROWSER_ALLOW_SCRIPT = 4u,
    MULIB_BROWSER_ALLOW_CROSS_ORIGIN = 8u,
    MULIB_BROWSER_ALLOW_FILE_URL = 16u
  } mulib_browser_permission_t;
  typedef struct mulib_browser_permissions
  {
    uint32_t allowed;
  } mulib_browser_permissions_t;
  /** @brief See manual/mulib-browser-permissions.md for this operation's
   * contract. */
  int mulib_browser_permissions_check (mulib_browser_permissions_t permissions,
                                       uint32_t required);
  int
  /** @brief See manual/mulib-browser-permissions.md for this operation's
     contract. */
  mulib_browser_permissions_grant (mulib_browser_permissions_t *permissions,
                                   uint32_t bits);
  void
  /** @brief See manual/mulib-browser-permissions.md for this operation's
     contract. */
  mulib_browser_permissions_revoke (mulib_browser_permissions_t *permissions,
                                    uint32_t bits);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_PERMISSIONS_H */
#ifdef MULIB_IMPL_BROWSER_PERMISSIONS
#ifndef MULIB_IMPL_BROWSER_PERMISSIONS_ONCE
#define MULIB_IMPL_BROWSER_PERMISSIONS_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_permissions_check (mulib_browser_permissions_t p, uint32_t need)
{
  return !(need & ~31u) && !(p.allowed & ~31u) && (p.allowed & need) == need;
}
int
mulib_browser_permissions_grant (mulib_browser_permissions_t *p,
                                 uint32_t flags)
{
  if (!p || flags & ~31u)
    return 0;
  p->allowed |= flags;
  return 1;
}
void
mulib_browser_permissions_revoke (mulib_browser_permissions_t *p,
                                  uint32_t flags)
{
  if (p)
    p->allowed &= ~flags;
}
#endif
#endif
