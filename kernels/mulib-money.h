/** @file mulib-money.h
 * @brief Checked integer-minor-unit monetary arithmetic and rational
 * conversions. C11 single-header module. Define MULIB_IMPL_MONEY in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_MONEY_H
#define MULIB_MONEY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_money
  {
    int64_t minor;
    char currency[4];
    uint8_t decimals;
  } mulib_money_t;
  /** @brief Validate an uppercase three-letter currency and 0..6 decimal
   * places. */
  int mulib_money_valid (mulib_money_t money);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_make (mulib_money_t *out, int64_t minor,
                        const char *currency, uint8_t decimals);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_add (mulib_money_t a, mulib_money_t b, mulib_money_t *out);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_sub (mulib_money_t a, mulib_money_t b, mulib_money_t *out);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_scale (mulib_money_t a, int64_t num, int64_t den,
                         mulib_money_t *out);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_parse (const char *decimal, const char *currency,
                         uint8_t digits, mulib_money_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MONEY_H */
#ifdef MULIB_IMPL_MONEY
#ifndef MULIB_IMPL_MONEY_ONCE
#define MULIB_IMPL_MONEY_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_money_valid (mulib_money_t a)
{
  return a.decimals <= 6 && a.currency[3] == 0 && a.currency[0] >= 'A'
         && a.currency[0] <= 'Z' && a.currency[1] >= 'A'
         && a.currency[1] <= 'Z' && a.currency[2] >= 'A'
         && a.currency[2] <= 'Z';
}
static int
mulib_money_same_ (mulib_money_t a, mulib_money_t b)
{
  return mulib_money_valid (a) && mulib_money_valid (b)
         && a.decimals == b.decimals && !memcmp (a.currency, b.currency, 4);
}
int
mulib_money_make (mulib_money_t *o, int64_t v, const char *c, uint8_t d)
{
  if (!o || !c || d > 6 || strlen (c) != 3)
    return 0;
  for (int i = 0; i < 3; i++)
    if (c[i] < 'A' || c[i] > 'Z')
      return 0;
  mulib_money_t x = { 0 };
  x.minor = v;
  memcpy (x.currency, c, 3);
  x.decimals = d;
  *o = x;
  return 1;
}
int
mulib_money_add (mulib_money_t a, mulib_money_t b, mulib_money_t *o)
{
  if (!o || !mulib_money_same_ (a, b)
      || ((b.minor > 0) && (a.minor > INT64_MAX - b.minor))
      || ((b.minor < 0) && (a.minor < INT64_MIN - b.minor)))
    return 0;
  a.minor += b.minor;
  *o = a;
  return 1;
}
int
mulib_money_sub (mulib_money_t a, mulib_money_t b, mulib_money_t *o)
{
  if (!o || !mulib_money_same_ (a, b)
      || ((b.minor < 0) && (a.minor > INT64_MAX + b.minor))
      || ((b.minor > 0) && (a.minor < INT64_MIN + b.minor)))
    return 0;
  a.minor -= b.minor;
  *o = a;
  return 1;
}
int
mulib_money_scale (mulib_money_t a, int64_t n, int64_t d, mulib_money_t *o)
{
  if (!o || !mulib_money_valid (a) || d <= 0 || n < 0)
    return 0;
  /* Binary multiply/divide without an overflowing intermediate product. */
  uint64_t magnitude
      = a.minor < 0 ? (uint64_t)(-(a.minor + 1)) + 1 : (uint64_t)a.minor;
  uint64_t den = (uint64_t)d, whole = magnitude / den, part = magnitude % den;
  uint64_t q = 0, r = 0,
           max = a.minor < 0 ? UINT64_C (9223372036854775808)
                             : (uint64_t)INT64_MAX;
  for (int bit = 62; bit >= 0; bit--)
    {
      if (q > max / 2)
        return 0;
      q *= 2;
      r *= 2;
      if (r >= den)
        {
          r -= den;
          if (q == max)
            return 0;
          q++;
        }
      if (((uint64_t)n >> bit) & 1)
        {
          if (whole > max - q)
            return 0;
          q += whole;
          r += part;
          if (r >= den)
            {
              r -= den;
              if (q == max)
                return 0;
              q++;
            }
        }
    }
  if (r >= den / 2 + den % 2)
    {
      if (q == max)
        return 0;
      q++;
    }
  a.minor = a.minor < 0 ? (q == UINT64_C (9223372036854775808) ? INT64_MIN
                                                               : -(int64_t)q)
                        : (int64_t)q;
  *o = a;
  return 1;
}
int
mulib_money_parse (const char *s, const char *c, uint8_t d, mulib_money_t *o)
{
  if (!s || !o || d > 6)
    return 0;
  int sign = 1;
  if (*s == '-')
    {
      sign = -1;
      s++;
    }
  else if (*s == '+')
    s++;
  if (!isdigit ((unsigned char)*s))
    return 0;
  uint64_t v = 0;
  while (isdigit ((unsigned char)*s))
    {
      unsigned z = (unsigned)(*s++ - '0');
      if (v > (UINT64_C (9223372036854775808) - z) / 10)
        return 0;
      v = v * 10 + z;
    }
  uint64_t mult = 1;
  for (unsigned i = 0; i < d; i++)
    mult *= 10;
  if (v > UINT64_C (9223372036854775808) / mult)
    return 0;
  v *= mult;
  if (*s == '.')
    {
      s++;
      if (!d || !isdigit ((unsigned char)*s))
        return 0;
      unsigned i = 0;
      while (isdigit ((unsigned char)*s))
        {
          if (i >= d)
            return 0;
          unsigned z = (unsigned)(*s++ - '0');
          uint64_t div = mult;
          for (unsigned j = 0; j <= i; j++)
            div /= 10;
          if ((uint64_t)z * div > UINT64_C (9223372036854775808) - v)
            return 0;
          v += (uint64_t)z * div;
          i++;
        }
    }
  if (*s || (sign > 0 && v > (uint64_t)INT64_MAX)
      || (sign < 0 && v > UINT64_C (9223372036854775808)))
    return 0;
  int64_t amt = sign < 0 ? (v == UINT64_C (9223372036854775808) ? INT64_MIN
                                                                : -(int64_t)v)
                         : (int64_t)v;
  return mulib_money_make (o, amt, c, d);
}
#endif
#endif
