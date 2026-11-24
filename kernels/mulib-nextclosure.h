/* MuLIB: Ganter NextClosure canonical enumeration of closed formal concepts. C11
 * single-header; define MULIB_IMPL_NEXTCLOSURE once. */
#ifndef MULIB_NEXTCLOSURE_H
#define MULIB_NEXTCLOSURE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-fca.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /* First closed intent is closure(empty). Returns 1 if a lectically next intent
   * exists. */
  uint64_t mulib_nextclosure_first (const mulib_fca_t *c);
  int mulib_nextclosure_next (const mulib_fca_t *c, uint64_t current, uint64_t *next);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NEXTCLOSURE) && !defined(MULIB_NEXTCLOSURE_IMPLEMENTED)
#define MULIB_NEXTCLOSURE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t
  mulib_nextclosure_first (const mulib_fca_t *c)
  {
    return mulib_fca_closure (c, 0);
  }
  int
  mulib_nextclosure_next (const mulib_fca_t *c, uint64_t a, uint64_t *next)
  {
    if (!c || !next || c->attributes > 64 || c->objects > 64
        || mulib_fca_closure (c, a) != a)
      return 0;
    for (size_t i = c->attributes; i > 0;)
      {
        --i;
        uint64_t bit = UINT64_C (1) << i;
        if (a & bit)
          continue;
        uint64_t prefix = i ? ((UINT64_C (1) << i) - 1) : 0;
        uint64_t b = mulib_fca_closure (c, (a & prefix) | bit);
        if ((b & prefix) == (a & prefix))
          {
            *next = b;
            return 1;
          }
      }
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif
