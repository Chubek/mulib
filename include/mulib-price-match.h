/** @file mulib-price-match.h
 * @brief ASCII-normalized product matching, identifiers and bounded edit
 * similarity. C11 single-header module. Define MULIB_IMPL_PRICE_MATCH in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_PRICE_MATCH_H
#define MULIB_PRICE_MATCH_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-price-match.md for this operation's contract. */
  int mulib_price_match_gtin (const char *digits);
  /** @brief See manual/mulib-price-match.md for this operation's contract. */
  int mulib_price_match_score (const char *a, const char *b, double *score);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_MATCH_H */
#ifdef MULIB_IMPL_PRICE_MATCH
#ifndef MULIB_IMPL_PRICE_MATCH_ONCE
#define MULIB_IMPL_PRICE_MATCH_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_match_gtin (const char *s)
{
  if (!s)
    return 0;
  size_t n = strlen (s);
  if (n != 8 && n != 12 && n != 13 && n != 14)
    return 0;
  int sum = 0;
  for (size_t i = 0; i < n; i++)
    {
      if (s[i] < '0' || s[i] > '9')
        return 0;
      if (i < n - 1)
        sum += (s[i] - '0') * (((n - 2 - i) % 2 == 0) ? 3 : 1);
    }
  return (10 - sum % 10) % 10 == s[n - 1] - '0';
}
static size_t
mulib_price_match_norm_ (const char *s, char *o, size_t cap)
{
  size_t n = 0;
  for (; *s && n + 1 < cap; s++)
    {
      unsigned char c = (unsigned char)*s;
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
          || (c >= '0' && c <= '9'))
        o[n++] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
  o[n] = 0;
  return n;
}
int
mulib_price_match_score (const char *a, const char *b, double *out)
{
  if (!a || !b || !out || strlen (a) > 1024 || strlen (b) > 1024)
    return 0;
  char x[1025], y[1025];
  size_t n = mulib_price_match_norm_ (a, x, sizeof x),
         m = mulib_price_match_norm_ (b, y, sizeof y);
  if (!n || !m)
    return 0;
  if (!strcmp (x, y))
    {
      *out = 1;
      return 1;
    }
  if (m > n)
    {
      char t[1025];
      memcpy (t, x, sizeof x);
      memcpy (x, y, sizeof x);
      memcpy (y, t, sizeof y);
      size_t z = n;
      n = m;
      m = z;
    }
  size_t *prev = malloc ((m + 1) * sizeof *prev),
         *cur = malloc ((m + 1) * sizeof *cur);
  if (!prev || !cur)
    {
      free (prev);
      free (cur);
      return 0;
    }
  for (size_t j = 0; j <= m; j++)
    prev[j] = j;
  for (size_t i = 1; i <= n; i++)
    {
      cur[0] = i;
      for (size_t j = 1; j <= m; j++)
        {
          size_t z = prev[j - 1] + (x[i - 1] != y[j - 1]);
          size_t q = prev[j] + 1;
          if (q < z)
            z = q;
          q = cur[j - 1] + 1;
          if (q < z)
            z = q;
          cur[j] = z;
        }
      size_t *t = prev;
      prev = cur;
      cur = t;
    }
  *out = 1.0 - (double)prev[m] / (double)n;
  free (prev);
  free (cur);
  return 1;
}
#endif
#endif
