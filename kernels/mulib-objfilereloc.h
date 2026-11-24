/** @file mulib-objfilereloc.h
 * @brief Checked little-endian ABS32/ABS64/PC32 relocation of byte spans.
 * Define MULIB_IMPL_OBJFILERELOC plus OBJFILEPOSINDEP and BYCUTILS. */
#ifndef MULIB_OBJFILERELOC_H
#define MULIB_OBJFILERELOC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_OBJFILERELOC_ABS32 = 1,
    MULIB_OBJFILERELOC_ABS64 = 2,
    MULIB_OBJFILERELOC_PC32 = 3
  };
  /** @brief Apply S+A (absolute) or S+A-P (relative) at offset; 1 success.
   * 0 invalid/overflow/range, without mutation. Caller resolves symbols. */
  int mulib_objfilereloc_apply (void *data, size_t size, size_t offset,
                                int kind, uint64_t symbol, int64_t addend,
                                uint64_t place);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILERELOC)                                          \
    && !defined(MULIB_OBJFILERELOC_IMPLEMENTED)
#define MULIB_OBJFILERELOC_IMPLEMENTED
#include "mulib-bycutils.h"
#include "mulib-objfileposindep.h"
int
mulib_objfilereloc_apply (void *p, size_t n, size_t o, int kind, uint64_t s,
                          int64_t a, uint64_t place)
{
  uint64_t m;
  int32_t d;
  if (kind == MULIB_OBJFILERELOC_PC32)
    {
      if (!mulib_objfileposindep_rel32 (s, a, place, &d))
        return 0;
      return mulib_bycutils_store (p, n, o, 4, (uint32_t)d);
    }
  if (kind != MULIB_OBJFILERELOC_ABS32 && kind != MULIB_OBJFILERELOC_ABS64)
    return 0;
  if (a < 0)
    {
      m = (uint64_t)(-(a + 1)) + 1;
      if (s < m)
        return 0;
      s -= m;
    }
  else
    {
      if (s > UINT64_MAX - (uint64_t)a)
        return 0;
      s += (uint64_t)a;
    }
  return mulib_bycutils_store (p, n, o,
                               kind == MULIB_OBJFILERELOC_ABS32 ? 4 : 8, s);
}
#endif
