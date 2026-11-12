/** @file mulib-shylock-core.h
 * @brief Bounded owned byte buffers, slices, validated parsing, hashing and
 * serialization helpers. C11 single-header module. Define
 * MULIB_IMPL_SHYLOCK_CORE in exactly one C translation unit.
 *
 */
#ifndef MULIB_SHYLOCK_CORE_H
#define MULIB_SHYLOCK_CORE_H
#include "mulib-utf8.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_shylock_buffer
  {
    char *data;
    size_t len, cap, limit;
  } mulib_shylock_buffer_t;
  typedef struct mulib_shylock_span
  {
    const char *data;
    size_t len;
  } mulib_shylock_span_t;
  /** @brief Reserve payload bytes (terminator excluded); zero limit is
   * unlimited. */
  int mulib_shylock_reserve (mulib_shylock_buffer_t *b, size_t n);
  /** @brief Append copied bytes, including self-slices; failure preserves
   * contents. */
  int mulib_shylock_append (mulib_shylock_buffer_t *b, const void *data,
                            size_t n);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_text (mulib_shylock_buffer_t *b, const char *s);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_json_string (mulib_shylock_buffer_t *b, const char *s,
                                 size_t n);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  void mulib_shylock_buffer_free (mulib_shylock_buffer_t *b);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  char *mulib_shylock_dup (const char *s, size_t n);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_copy (char *dst, size_t cap, const char *s);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  uint64_t mulib_shylock_hash (const void *data, size_t len);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_equal_ci (const char *a, const char *b);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_contains_ci (const char *s, const char *needle);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_contains_span_ci (const char *s, size_t n,
                                      const char *needle);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_parse_decimal (const char *s, double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SHYLOCK_CORE_H */
#ifdef MULIB_IMPL_SHYLOCK_CORE
#ifndef MULIB_IMPL_SHYLOCK_CORE_ONCE
#define MULIB_IMPL_SHYLOCK_CORE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_shylock_reserve (mulib_shylock_buffer_t *b, size_t n)
{
  if (!b || (b->cap && b->len >= b->cap) || (b->len && !b->cap)
      || (b->cap && !b->data) || n == SIZE_MAX || (b->limit && n > b->limit))
    return 0;
  if (n + 1 <= b->cap)
    return 1;
  size_t c = b->cap ? b->cap : 64;
  while (c < n + 1)
    {
      if (c > SIZE_MAX / 2)
        {
          c = n + 1;
          break;
        }
      c *= 2;
    }
  if (b->limit && b->limit < SIZE_MAX && c > b->limit + 1)
    c = b->limit + 1;
  char *p = (char *)realloc (b->data, c);
  if (!p)
    return 0;
  b->data = p;
  b->cap = c;
  b->data[b->len] = 0;
  return 1;
}
int
mulib_shylock_append (mulib_shylock_buffer_t *b, const void *p, size_t n)
{
  if (!b || (!p && n) || b->len > SIZE_MAX - n)
    return 0;
  size_t offset = 0;
  int alias = 0;
  if (n && b->data && (uintptr_t)p >= (uintptr_t)b->data
      && (uintptr_t)p - (uintptr_t)b->data < b->cap)
    {
      offset = (size_t)((uintptr_t)p - (uintptr_t)b->data);
      if (offset > b->len || n > b->len - offset)
        return 0;
      alias = 1;
    }
  if (!mulib_shylock_reserve (b, b->len + n))
    return 0;
  if (alias)
    p = b->data + offset;
  if (n)
    {
      memmove (b->data + b->len, p, n);
    }
  b->len += n;
  b->data[b->len] = 0;
  return 1;
}
int
mulib_shylock_text (mulib_shylock_buffer_t *b, const char *s)
{
  return s && mulib_shylock_append (b, s, strlen (s));
}
int
mulib_shylock_json_string (mulib_shylock_buffer_t *out, const char *s,
                           size_t n)
{
  if (!out || (!s && n)
      || (n && !mulib_utf8_validate ((const unsigned char *)s, n)))
    return 0;
  mulib_shylock_buffer_t tmp = { 0 };
  tmp.limit = out->limit;
  mulib_shylock_buffer_t *b = &tmp;
  int ok = 0;
  if (!mulib_shylock_text (b, "\""))
    goto done;
  for (size_t i = 0; i < n; i++)
    {
      unsigned char c = (unsigned char)s[i];
      if (c == '"' || c == '\\')
        {
          char x[2] = { '\\', (char)c };
          if (!mulib_shylock_append (b, x, 2))
            goto done;
        }
      else if (c < 32)
        {
          char esc[7];
          int k = snprintf (esc, sizeof esc, "\\u%04x", (unsigned)c);
          if (k != 6 || !mulib_shylock_append (b, esc, 6))
            goto done;
        }
      else if (!mulib_shylock_append (b, &c, 1))
        goto done;
    }
  if (mulib_shylock_text (b, "\""))
    ok = mulib_shylock_append (out, b->data, b->len);
done:
  mulib_shylock_buffer_free (b);
  return ok;
}
void
mulib_shylock_buffer_free (mulib_shylock_buffer_t *b)
{
  if (b)
    {
      free (b->data);
      memset (b, 0, sizeof *b);
    }
}
char *
mulib_shylock_dup (const char *s, size_t n)
{
  if ((!s && n) || n == SIZE_MAX)
    return NULL;
  char *p = (char *)malloc (n + 1);
  if (!p)
    return NULL;
  if (n)
    memcpy (p, s, n);
  p[n] = 0;
  return p;
}
int
mulib_shylock_copy (char *d, size_t cap, const char *s)
{
  if (!d || !s)
    return 0;
  size_t n = strlen (s);
  if (n >= cap)
    return 0;
  memmove (d, s, n + 1);
  return 1;
}
uint64_t
mulib_shylock_hash (const void *p, size_t n)
{
  uint64_t h = UINT64_C (14695981039346656037);
  const unsigned char *s = p;
  if (!p && n)
    return 0;
  for (size_t i = 0; i < n; i++)
    {
      h ^= s[i];
      h *= UINT64_C (1099511628211);
    }
  return h;
}
int
mulib_shylock_equal_ci (const char *a, const char *b)
{
  if (!a || !b)
    return 0;
  while (*a && *b)
    {
      if (tolower ((unsigned char)*a) != tolower ((unsigned char)*b))
        return 0;
      a++;
      b++;
    }
  return !*a && !*b;
}
int
mulib_shylock_contains_ci (const char *s, const char *n)
{
  if (!s || !n)
    return 0;
  size_t k = strlen (n);
  for (; *s; s++)
    {
      size_t i = 0;
      while (i < k && s[i]
             && tolower ((unsigned char)s[i]) == tolower ((unsigned char)n[i]))
        i++;
      if (i == k)
        return 1;
    }
  return !k;
}
int
mulib_shylock_contains_span_ci (const char *s, size_t n, const char *needle)
{
  if ((!s && n) || !needle)
    return 0;
  size_t k = strlen (needle);
  if (!k)
    return 1;
  if (k > n)
    return 0;
  for (size_t j = 0; j <= n - k; j++)
    {
      size_t i = 0;
      while (i < k
             && tolower ((unsigned char)s[j + i])
                    == tolower ((unsigned char)needle[i]))
        i++;
      if (i == k)
        return 1;
    }
  return 0;
}
int
mulib_shylock_parse_decimal (const char *s, double *out)
{
  if (!s || !out)
    return 0;
  while (isspace ((unsigned char)*s))
    s++;
  int neg = 0, nonzero = 0;
  if (*s == '+' || *s == '-')
    neg = *s++ == '-';
  long double v = 0, scale = 1;
  size_t digits = 0;
  while (*s >= '0' && *s <= '9')
    {
      nonzero |= *s != '0';
      v = v * 10 + (*s++ - '0');
      digits++;
      if (!isfinite (v))
        return 0;
    }
  if (*s == '.')
    {
      s++;
      while (*s >= '0' && *s <= '9')
        {
          nonzero |= *s != '0';
          scale *= .1L;
          v += (*s++ - '0') * scale;
          digits++;
        }
    }
  if (!digits)
    return 0;
  int exponent = 0, eneg = 0;
  if (*s == 'e' || *s == 'E')
    {
      s++;
      if (*s == '+' || *s == '-')
        eneg = *s++ == '-';
      if (*s < '0' || *s > '9')
        return 0;
      while (*s >= '0' && *s <= '9')
        {
          if (exponent > 10000)
            return 0;
          exponent = exponent * 10 + (*s++ - '0');
        }
    }
  while (isspace ((unsigned char)*s))
    s++;
  if (*s)
    return 0;
  if (v != 0)
    v *= powl (10.L, eneg ? -exponent : exponent);
  double result = (double)(neg ? -v : v);
  if (!isfinite (result) || (nonzero && result == 0))
    return 0;
  *out = result;
  return 1;
}
#endif
#endif
