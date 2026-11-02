/** @file mulib-seurity.h
 * @brief Security-oriented byte comparison, HTTP tokens and safe origin paths.
 * Historical filename retained. No cryptography or authentication policy.
 * Define MULIB_IMPL_SEURITY (or the historical MULIB_IMPL_SEURITY) once. */
#ifndef MULIB_SEURITY_H
#define MULIB_SEURITY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_security_equal (const void *a, const void *b, size_t length);
  int mulib_security_token (const char *text);
  /** Decode percent escapes once, reject controls, backslashes, query/fragment,
   * encoded separators, dot segments and repeated slashes. Absolute paths only.
   * Output may alias input, is NUL terminated and unchanged on failure. */
  int mulib_security_path (const char *source, char *out, size_t capacity);
  int mulib_security_ascii_equal (const char *a, const char *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEURITY) && !defined(MULIB_IMPL_SECURITY)
#define MULIB_IMPL_SECURITY
#endif
#if defined(MULIB_IMPL_SECURITY) && !defined(MULIB_SEURITY_IMPLEMENTED)
#define MULIB_SEURITY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_security_equal (const void *a, const void *b, size_t n)
{
  const unsigned char *x = (const unsigned char *)a,
                      *y = (const unsigned char *)b;
  volatile unsigned diff = 0;
  if ((!a || !b) && n)
    return 0;
  for (size_t i = 0; i < n; ++i)
    diff |= x[i] ^ y[i];
  return diff == 0;
}
int
mulib_security_ascii_equal (const char *a, const char *b)
{
  if (!a || !b)
    return 0;
  while (*a && *b)
    {
      unsigned x = (unsigned char)*a++, y = (unsigned char)*b++;
      if (x >= 'A' && x <= 'Z')
        x += 32;
      if (y >= 'A' && y <= 'Z')
        y += 32;
      if (x != y)
        return 0;
    }
  return *a == *b;
}
int
mulib_security_token (const char *s)
{
  if (!s || !*s)
    return 0;
  for (; *s; ++s)
    {
      unsigned c = (unsigned char)*s;
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || strchr ("!#$%&'*+-.^_`|~", (int)c)))
        return 0;
    }
  return 1;
}
static int
mulib_security_hex_ (unsigned c)
{
  if (c >= '0' && c <= '9')
    return (int)(c - '0');
  if (c >= 'a' && c <= 'f')
    return (int)(c - 'a' + 10);
  if (c >= 'A' && c <= 'F')
    return (int)(c - 'A' + 10);
  return -1;
}
int
mulib_security_path (const char *s, char *out, size_t cap)
{
  size_t n, j = 0, start = 1;
  char *p;
  if (!s || !out || s[0] != '/' || !(n = strlen (s)) || n >= cap)
    return 0;
  p = (char *)malloc (n + 1);
  if (!p)
    return 0;
  for (size_t i = 0; i < n; ++i)
    {
      unsigned c = (unsigned char)s[i];
      if (c == '%')
        {
          int a, b;
          if (n - i < 3
              || (a = mulib_security_hex_ ((unsigned char)s[i + 1])) < 0
              || (b = mulib_security_hex_ ((unsigned char)s[i + 2])) < 0)
            goto invalid;
          c = (unsigned)(a * 16 + b);
          i += 2;
          if (c == '/' || c == '%' || c == '\\')
            goto invalid;
        }
      if (c < 32 || c == 127 || c == '\\' || c == '?' || c == '#')
        goto invalid;
      p[j++] = (char)c;
    }
  p[j] = 0;
  for (size_t i = 1; i <= j; ++i)
    if (i == j || p[i] == '/')
      {
        size_t len = i - start;
        if ((!len && i != j) || (len == 1 && p[start] == '.')
            || (len == 2 && p[start] == '.' && p[start + 1] == '.'))
          goto invalid;
        start = i + 1;
      }
  memcpy (out, p, j + 1);
  free (p);
  return 1;
invalid:
  free (p);
  return 0;
}
#endif
