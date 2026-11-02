/** @file mulib-auth.h
 * @brief Bounded HTTP Basic decoding and Bearer extraction; no password store.
 * Define MULIB_IMPL_AUTH, BASE64 and SECURITY. Header values are borrowed.
 * Caller must require an authenticated TLS transport before using credentials.
 */
#ifndef MULIB_AUTH_H
#define MULIB_AUTH_H
#include "mulib-base64.h"
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Borrow token, NULL for malformed/wrong scheme. */
  const char *mulib_auth_bearer (const char *authorization);
  /** Copy decoded username:password into out, NUL terminated; length excludes
   * NUL. Reject embedded NUL/control bytes and missing colon. Failure
   * unchanged. */
  int mulib_auth_basic (const char *authorization, char *out, size_t capacity,
                        size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AUTH) && !defined(MULIB_AUTH_IMPLEMENTED)
#define MULIB_AUTH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
static const char *
mulib_auth_scheme_ (const char *s, const char *scheme)
{
  char prefix[8];
  size_t n = strlen (scheme);
  if (!s || strlen (s) <= n || n >= sizeof (prefix))
    return NULL;
  memcpy (prefix, s, n);
  prefix[n] = 0;
  return mulib_security_ascii_equal (prefix, scheme) && s[n] == ' ' ? s + n + 1
                                                                    : NULL;
}
const char *
mulib_auth_bearer (const char *s)
{
  const char *p = mulib_auth_scheme_ (s, "Bearer"), *start = p;
  int padding = 0;
  if (!p || !*p)
    return NULL;
  for (; *p; ++p)
    {
      unsigned c = (unsigned char)*p;
      if (c == '=')
        {
          padding = 1;
          continue;
        }
      if (padding
          || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
               || (c >= '0' && c <= '9') || strchr ("-._~+/", (int)c)))
        return NULL;
    }
  return *start == '=' ? NULL : start;
}
int
mulib_auth_basic (const char *s, char *out, size_t cap, size_t *length)
{
  const char *p = mulib_auth_scheme_ (s, "Basic");
  unsigned char *tmp;
  size_t n, used;
  if (!p || !out || !cap || !(n = strlen (p)) || n / 4 > (SIZE_MAX - 1) / 3)
    return 0;
  tmp = (unsigned char *)malloc (n / 4 * 3 + 1);
  if (!tmp)
    return 0;
  if (!mulib_base64_decode (p, n, tmp, n / 4 * 3, &used) || used >= cap
      || !memchr (tmp, ':', used))
    {
      free (tmp);
      return 0;
    }
  for (size_t i = 0; i < used; ++i)
    if (tmp[i] < 32 || tmp[i] == 127)
      {
        free (tmp);
        return 0;
      }
  tmp[used] = 0;
  memcpy (out, tmp, used + 1);
  free (tmp);
  if (length)
    *length = used;
  return 1;
}
#endif
