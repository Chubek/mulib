/** @file mulib-browser-snapshot.h
 * @brief Owned immutable byte snapshots with content fingerprint and document
 * version. C11 single-header module. Define MULIB_IMPL_BROWSER_SNAPSHOT in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_SNAPSHOT_H
#define MULIB_BROWSER_SNAPSHOT_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_snapshot
  {
    mulib_shylock_buffer_t bytes;
    uint64_t version, hash;
  } mulib_browser_snapshot_t;
  /** @brief See manual/mulib-browser-snapshot.md for this operation's
   * contract. */
  int mulib_browser_snapshot_make (mulib_browser_snapshot_t *out,
                                   const void *bytes, size_t len,
                                   uint64_t version, size_t max_bytes);
  /** @brief See manual/mulib-browser-snapshot.md for this operation's
   * contract. */
  int mulib_browser_snapshot_equal (const mulib_browser_snapshot_t *a,
                                    const mulib_browser_snapshot_t *b);
  /** @brief See manual/mulib-browser-snapshot.md for this operation's
   * contract. */
  void mulib_browser_snapshot_free (mulib_browser_snapshot_t *s);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_SNAPSHOT_H */
#ifdef MULIB_IMPL_BROWSER_SNAPSHOT
#ifndef MULIB_IMPL_BROWSER_SNAPSHOT_ONCE
#define MULIB_IMPL_BROWSER_SNAPSHOT_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_snapshot_make (mulib_browser_snapshot_t *o, const void *p,
                             size_t n, uint64_t ver, size_t max)
{
  if (!o || (!p && n) || (max && n > max))
    return 0;
  mulib_browser_snapshot_t s = { 0 };
  s.bytes.limit = max;
  if (!mulib_shylock_append (&s.bytes, p, n))
    return 0;
  s.hash = mulib_shylock_hash (p, n);
  s.version = ver;
  *o = s;
  return 1;
}
int
mulib_browser_snapshot_equal (const mulib_browser_snapshot_t *a,
                              const mulib_browser_snapshot_t *b)
{
  return a && b && a->bytes.len == b->bytes.len && a->hash == b->hash
         && (!a->bytes.len
             || !memcmp (a->bytes.data, b->bytes.data, a->bytes.len));
}
void
mulib_browser_snapshot_free (mulib_browser_snapshot_t *s)
{
  if (s)
    {
      mulib_shylock_buffer_free (&s->bytes);
      memset (s, 0, sizeof *s);
    }
}
#endif
#endif
