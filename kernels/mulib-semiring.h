/* MuLIB: Boolean, nonnegative integral-double and tropical semirings.
 * Natural overflow saturates to +infinity; integers above 2^53 round.
 * NaN, invalid kinds and negative/fractional natural operands return NaN. */
#ifndef MULIB_SEMIRING_H
#define MULIB_SEMIRING_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_SEMIRING_BOOLEAN,
    MULIB_SEMIRING_NATURAL,
    MULIB_SEMIRING_MIN_PLUS,
    MULIB_SEMIRING_MAX_PLUS
  } mulib_semiring_kind_t;
  double mulib_semiring_zero (mulib_semiring_kind_t kind);
  double mulib_semiring_one (mulib_semiring_kind_t kind);
  double mulib_semiring_add (mulib_semiring_kind_t kind, double a, double b);
  double mulib_semiring_mul (mulib_semiring_kind_t kind, double a, double b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEMIRING) && !defined(MULIB_SEMIRING_IMPLEMENTED)
#define MULIB_SEMIRING_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_semiring_valid_ (mulib_semiring_kind_t k, double a, double b)
  {
    return !isnan (a) && !isnan (b)
           && (k != MULIB_SEMIRING_NATURAL
               || (a >= 0 && b >= 0 && floor (a) == a && floor (b) == b));
  }
  double
  mulib_semiring_zero (mulib_semiring_kind_t k)
  {
    switch (k)
      {
      case MULIB_SEMIRING_BOOLEAN:
      case MULIB_SEMIRING_NATURAL:
        return 0;
      case MULIB_SEMIRING_MIN_PLUS:
        return INFINITY;
      case MULIB_SEMIRING_MAX_PLUS:
        return -INFINITY;
      default:
        return NAN;
      }
  }
  double
  mulib_semiring_one (mulib_semiring_kind_t k)
  {
    switch (k)
      {
      case MULIB_SEMIRING_BOOLEAN:
      case MULIB_SEMIRING_NATURAL:
        return 1;
      case MULIB_SEMIRING_MIN_PLUS:
      case MULIB_SEMIRING_MAX_PLUS:
        return 0;
      default:
        return NAN;
      }
  }
  double
  mulib_semiring_add (mulib_semiring_kind_t k, double a, double b)
  {
    if (!mulib_semiring_valid_ (k, a, b))
      return NAN;
    switch (k)
      {
      case MULIB_SEMIRING_BOOLEAN:
        return (a != 0) || (b != 0);
      case MULIB_SEMIRING_NATURAL:
        return a + b;
      case MULIB_SEMIRING_MIN_PLUS:
        return fmin (a, b);
      case MULIB_SEMIRING_MAX_PLUS:
        return fmax (a, b);
      default:
        return NAN;
      }
  }
  double
  mulib_semiring_mul (mulib_semiring_kind_t k, double a, double b)
  {
    if (!mulib_semiring_valid_ (k, a, b))
      return NAN;
    switch (k)
      {
      case MULIB_SEMIRING_BOOLEAN:
        return (a != 0) && (b != 0);
      case MULIB_SEMIRING_NATURAL:
        return a == 0 || b == 0 ? 0 : a * b;
      case MULIB_SEMIRING_MIN_PLUS:
        return a == INFINITY || b == INFINITY ? INFINITY : a + b;
      case MULIB_SEMIRING_MAX_PLUS:
        return a == -INFINITY || b == -INFINITY ? -INFINITY : a + b;
      default:
        return NAN;
      }
  }
#ifdef __cplusplus
}
#endif
#endif
