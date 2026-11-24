/** @file mulib-rewrite_url.h
 * @brief Component-prefix origin-path rewrite preserving a raw query.
 * Define MULIB_IMPL_REWRITE_URL and SECURITY. No regex or redirect generation.
 */
#ifndef MULIB_REWRITE_URL_H
#define MULIB_REWRITE_URL_H
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** 1 rewritten (malloc-owned output), 0 no match, -1 invalid/OOM.
   * Prefix and replacement are strict absolute paths without trailing slash,
   * except root. Output unchanged unless 1. */
  int mulib_rewrite_url (const char *target, const char *prefix,
                         const char *replacement, char **out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_REWRITE_URL) && !defined(MULIB_REWRITE_URL_IMPLEMENTED)
#define MULIB_REWRITE_URL_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int
mulib_rewrite_url (const char *s, const char *from, const char *to, char **out)
{
  size_t n, a, b, path, skip, tail;
  char *tmp, *result;
  if (!s || !from || !to || !out)
    return -1;
  n = strlen (s);
  a = strlen (from);
  b = strlen (to);
  path = strcspn (s, "?");
  if (!a || !b || (a > 1 && from[a - 1] == '/') || (b > 1 && to[b - 1] == '/')
      || strchr (s, '#'))
    return -1;
  tmp = (char *)malloc ((n > a ? (n > b ? n : b) : (a > b ? a : b)) + 1);
  if (!tmp)
    return -1;
  if (!mulib_security_path (from, tmp, a + 1)
      || !mulib_security_path (to, tmp, b + 1))
    {
      free (tmp);
      return -1;
    }
  memcpy (tmp, s, path);
  tmp[path] = 0;
  if (!mulib_security_path (tmp, tmp, path + 1))
    {
      free (tmp);
      return -1;
    }
  free (tmp);
  if (path < a || memcmp (s, from, a) || (a > 1 && path > a && s[a] != '/'))
    return 0;
  skip = a == 1 ? 0 : a;
  if (b == 1 && skip < path && s[skip] == '/')
    ++skip;
  if (a == 1 && b > 1)
    skip = 0;
  tail = n - skip;
  if (tail > SIZE_MAX - b - 1)
    return -1;
  result = (char *)malloc (b + tail + 1);
  if (!result)
    return -1;
  memcpy (result, to, b);
  memcpy (result + b, s + skip, tail + 1);
  *out = result;
  return 1;
}
#endif
