/** @file mulib-webidl-runtime.h
 * @brief Strict primitive WebIDL conversion utilities with range validation.
 * C11 single-header module. Define MULIB_IMPL_WEBIDL_RUNTIME in exactly one C
 * translation unit. Offers a conservative primitive subset, not complete
 * WebIDL conversion.
 */
#ifndef MULIB_WEBIDL_RUNTIME_H
#define MULIB_WEBIDL_RUNTIME_H
#include "mulib-shylock-core.h"
#include "mulib-utf8.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-webidl-runtime.md for this operation's contract.
   */
  int mulib_webidl_to_int32 (double value, int clamp, int32_t *out);
  /** @brief See manual/mulib-webidl-runtime.md for this operation's contract.
   */
  int mulib_webidl_to_uint32 (double value, int clamp, uint32_t *out);
  /** @brief See manual/mulib-webidl-runtime.md for this operation's contract.
   */
  int mulib_webidl_to_boolean (double number);
  /** @brief See manual/mulib-webidl-runtime.md for this operation's contract.
   */
  int mulib_webidl_domstring_utf8 (const char *utf8, size_t n,
                                   mulib_shylock_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBIDL_RUNTIME_H */
#ifdef MULIB_IMPL_WEBIDL_RUNTIME
#ifndef MULIB_IMPL_WEBIDL_RUNTIME_ONCE
#define MULIB_IMPL_WEBIDL_RUNTIME_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double
mulib_webidl_round_ (double x)
{
  double f = floor (x), r = x - f;
  return r > .5 || (r == .5 && fmod (f, 2) != 0) ? f + 1 : f;
}
int
mulib_webidl_to_int32 (double x, int clamp, int32_t *o)
{
  if (!o || !isfinite (x))
    return 0;
  if (clamp)
    {
      x = mulib_webidl_round_ (
          fmax ((double)INT32_MIN, fmin ((double)INT32_MAX, x)));
    }
  else if (x < INT32_MIN || x > INT32_MAX || trunc (x) != x)
    return 0;
  *o = (int32_t)x;
  return 1;
}
int
mulib_webidl_to_uint32 (double x, int clamp, uint32_t *o)
{
  if (!o || !isfinite (x))
    return 0;
  if (clamp)
    x = mulib_webidl_round_ (fmax (0, fmin ((double)UINT32_MAX, x)));
  else if (x < 0 || x > UINT32_MAX || trunc (x) != x)
    return 0;
  *o = (uint32_t)x;
  return 1;
}
int
mulib_webidl_to_boolean (double n)
{
  return n != 0 && !isnan (n);
}
int
mulib_webidl_domstring_utf8 (const char *s, size_t n,
                             mulib_shylock_buffer_t *out)
{
  if ((!s && n) || !out
      || (n && !mulib_utf8_validate ((const unsigned char *)s, n)))
    return 0;
  return mulib_shylock_append (out, s, n);
}
#endif
#endif
