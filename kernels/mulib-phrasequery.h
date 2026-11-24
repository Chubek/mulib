/* MuLIB: Ordered phrase matching over positional posting arrays. C11 single-header;
 * define MULIB_IMPL_PHRASEQUERY once. */
#ifndef MULIB_PHRASEQUERY_H
#define MULIB_PHRASEQUERY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-positions.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /* positions[i] are sorted token locations of phrase token i; exact ordered phrase. */
  int mulib_phrasequery_match (const mulib_positions_t *positions, size_t token_count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PHRASEQUERY) && !defined(MULIB_PHRASEQUERY_IMPLEMENTED)
#define MULIB_PHRASEQUERY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_phrasequery_match (const mulib_positions_t *p, size_t count)
  {
    if (!p || !count || count > UINT32_MAX)
      return 0;
    for (size_t i = 0; i < p[0].count; i++)
      {
        uint32_t start = p[0].items[i];
        int ok = 1;
        for (size_t t = 1; t < count; t++)
          {
            if (start > UINT32_MAX - t)
              {
                ok = 0;
                break;
              }
            uint32_t target = start + (uint32_t)t;
            size_t lo = 0, hi = p[t].count;
            while (lo < hi)
              {
                size_t m = lo + (hi - lo) / 2;
                if (p[t].items[m] < target)
                  lo = m + 1;
                else
                  hi = m;
              }
            if (lo == p[t].count || p[t].items[lo] != target)
              {
                ok = 0;
                break;
              }
          }
        if (ok)
          return 1;
      }
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif
