/** @file mulib-objfileposindep.h
 * @brief Overflow-checked PC-relative signed 32-bit displacement arithmetic.
 * Define MULIB_IMPL_OBJFILEPOSINDEP once. P denotes relocation field address.
 */
#ifndef MULIB_OBJFILEPOSINDEP_H
#define MULIB_OBJFILEPOSINDEP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Compute S+A-P without signed overflow; 1 success, unchanged on
   * error. S+A must be representable as an unsigned 64-bit address. */
  int mulib_objfileposindep_rel32 (uint64_t symbol, int64_t addend,
                                   uint64_t place, int32_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILEPOSINDEP)                                       \
    && !defined(MULIB_OBJFILEPOSINDEP_IMPLEMENTED)
#define MULIB_OBJFILEPOSINDEP_IMPLEMENTED
int
mulib_objfileposindep_rel32 (uint64_t s, int64_t a, uint64_t p, int32_t *out)
{
  uint64_t magnitude, d;
  if (!out)
    return 0;
  if (a < 0)
    {
      magnitude = (uint64_t)(-(a + 1)) + 1;
      if (s < magnitude)
        return 0;
      s -= magnitude;
    }
  else
    {
      if (s > UINT64_MAX - (uint64_t)a)
        return 0;
      s += (uint64_t)a;
    }
  if (s >= p)
    {
      d = s - p;
      if (d > INT32_MAX)
        return 0;
      *out = (int32_t)d;
    }
  else
    {
      d = p - s;
      if (d > UINT64_C (2147483648))
        return 0;
      *out = d == UINT64_C (2147483648) ? INT32_MIN : -(int32_t)d;
    }
  return 1;
}
#endif
