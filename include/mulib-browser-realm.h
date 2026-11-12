/** @file mulib-browser-realm.h
 * @brief Browser realm IDs with immutable origin metadata and isolation keys.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_REALM in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_REALM_H
#define MULIB_BROWSER_REALM_H
#include "mulib-browser-origin.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_realm
  {
    uint64_t id, context_id;
    char origin[256];
    uint32_t flags;
  } mulib_browser_realm_t;
  /** @brief See manual/mulib-browser-realm.md for this operation's contract.
   */
  int mulib_browser_realm_init (mulib_browser_realm_t *out, uint64_t id,
                                uint64_t context, const char *origin,
                                unsigned flags);
  /** @brief See manual/mulib-browser-realm.md for this operation's contract.
   */
  int mulib_browser_realm_same_origin (const mulib_browser_realm_t *a,
                                       const mulib_browser_realm_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_REALM_H */
#ifdef MULIB_IMPL_BROWSER_REALM
#ifndef MULIB_IMPL_BROWSER_REALM_ONCE
#define MULIB_IMPL_BROWSER_REALM_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_realm_init (mulib_browser_realm_t *o, uint64_t id, uint64_t ctx,
                          const char *origin, unsigned flags)
{
  if (!o || !id || !ctx)
    return 0;
  mulib_browser_origin_t parsed;
  if (!mulib_browser_origin_parse (origin, &parsed))
    return 0;
  mulib_browser_realm_t z = { id, ctx, { 0 }, flags };
  int ipv6 = strchr (parsed.host, ':') != NULL;
  int n = snprintf (z.origin, sizeof z.origin, "%s://%s%s%s:%u", parsed.scheme,
                    ipv6 ? "[" : "", parsed.host, ipv6 ? "]" : "",
                    (unsigned)parsed.port);
  if (n < 0 || (size_t)n >= sizeof z.origin)
    return 0;
  *o = z;
  return 1;
}
int
mulib_browser_realm_same_origin (const mulib_browser_realm_t *a,
                                 const mulib_browser_realm_t *b)
{
  return a && b && a->id && b->id && memchr (a->origin, 0, sizeof a->origin)
         && memchr (b->origin, 0, sizeof b->origin) && a->origin[0]
         && !strcmp (a->origin, b->origin);
}
#endif
#endif
