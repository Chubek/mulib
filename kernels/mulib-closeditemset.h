/* MuLIB: Frequent closed intents of an FCA context with minimum support. C11
 * single-header; define MULIB_IMPL_CLOSEDITEMSET once. */
#ifndef MULIB_CLOSEDITEMSET_H
#define MULIB_CLOSEDITEMSET_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-nextclosure.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_closeditemset_visit_fn) (uint64_t attributes,
                                               uint64_t object_extent, void *context);
  /* Exact closed-set enumeration; callbacks return nonzero to continue. */
  size_t mulib_closeditemset_enumerate (const mulib_fca_t *c, size_t min_support,
                                        mulib_closeditemset_visit_fn visit,
                                        void *context);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CLOSEDITEMSET) && !defined(MULIB_CLOSEDITEMSET_IMPLEMENTED)
#define MULIB_CLOSEDITEMSET_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  static size_t
  mulib_closeditemset_pop_ (uint64_t x)
  {
    size_t n = 0;
    while (x)
      {
        x &= x - 1;
        n++;
      }
    return n;
  }
  size_t
  mulib_closeditemset_enumerate (const mulib_fca_t *c, size_t min,
                                 mulib_closeditemset_visit_fn f, void *ctx)
  {
    if (!c || !f || c->objects > 64 || c->attributes > 64)
      return SIZE_MAX;
    if (min > c->objects)
      return 0;
    uint64_t a = mulib_nextclosure_first (c);
    size_t count = 0;
    for (;;)
      {
        uint64_t e = mulib_fca_extent (c, a);
        if (mulib_closeditemset_pop_ (e) >= min)
          {
            if (count == SIZE_MAX - 1)
              return SIZE_MAX;
            count++;
            if (!f (a, e, ctx))
              break;
          }
        uint64_t next;
        if (!mulib_nextclosure_next (c, a, &next))
          break;
        a = next;
      }
    return count;
  }
#ifdef __cplusplus
}
#endif
#endif
