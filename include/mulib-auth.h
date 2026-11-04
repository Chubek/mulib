/** @file mulib-auth.h
 * @brief Bounded HTTP Basic/Bearer authentication and opaque API-key checking.
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
  /** Maximum accepted header length, excluding NUL. */
#define MULIB_AUTH_MAX_HEADER 8192u
  /** Copy decoded username:password into out, NUL terminated; length excludes
   * NUL. Reject embedded NUL/control bytes and missing colon. Failure
   * unchanged. */
  int mulib_auth_basic (const char *authorization, char *out, size_t capacity,
                        size_t *length);
  /** Length-delimited Basic decoder; embedded NUL is rejected. */
  int mulib_auth_basic_n (const char *authorization, size_t header_length,
                          char *out, size_t capacity, size_t *length);
  typedef int (*mulib_auth_basic_verify_fn) (void *context,
                                             const char *user, size_t user_length,
                                             const char *password, size_t password_length);
  typedef int (*mulib_auth_token_verify_fn) (void *context, const char *token);
  /** Decode, invoke policy, then wipe temporary credentials. Only a verifier
   * return value of exactly 1 authorizes. Credential slices are call-borrowed. */
  int mulib_auth_basic_authorize (const char *authorization,
                                  mulib_auth_basic_verify_fn verify, void *context);
  /** Parse before invoking verifier. NULL policy denies; never merely checks
   * that a Bearer header exists. */
  int mulib_auth_bearer_authorize (const char *authorization,
                                   mulib_auth_token_verify_fn verify, void *context);
  /** Compare opaque, nonempty binary API keys (at most MAX_HEADER bytes), with
   * no data-dependent early exit at equal lengths. Use high-entropy keys, TLS,
   * revocation and rate limits. This does not hash keys for storage. */
  int mulib_auth_apikey (const void *expected, size_t expected_length,
                         const void *received, size_t received_length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AUTH) && !defined(MULIB_AUTH_IMPLEMENTED)
#define MULIB_AUTH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
static size_t
mulib_auth_length_ (const char *s)
{
  size_t n=0;
  if (!s) return 0;
  while (n<=MULIB_AUTH_MAX_HEADER && s[n]) ++n;
  return n<=MULIB_AUTH_MAX_HEADER ? n : 0;
}
static const char *
mulib_auth_scheme_ (const char *s, size_t length, const char *scheme)
{
  size_t n=strlen(scheme);
  if (!s || length<=n || length>MULIB_AUTH_MAX_HEADER) return NULL;
  for (size_t i=0; i<n; ++i) {
    unsigned c=(unsigned char)s[i];
    if (c>='A' && c<='Z') c+=32;
    if (c!=(unsigned char)scheme[i]) return NULL;
  }
  if (s[n]!=' ') return NULL;
  while (n<length && s[n]==' ') ++n;
  return n<length ? s+n : NULL;
}
static void
mulib_auth_wipe_ (void *p, size_t n)
{
  volatile unsigned char *v=(volatile unsigned char *)p;
  while (n--) *v++=0;
}
const char *
mulib_auth_bearer (const char *s)
{
  const char *p = mulib_auth_scheme_ (s, mulib_auth_length_(s), "bearer"), *start = p;
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
  return mulib_auth_basic_n(s,mulib_auth_length_(s),out,cap,length);
}
int
mulib_auth_basic_n (const char *s, size_t hn, char *out, size_t cap, size_t *length)
{
  const char *p = mulib_auth_scheme_ (s, hn, "basic");
  unsigned char *tmp;
  size_t n, used, allocated;
  if (!p || !out || !cap || memchr(s,0,hn))
    return 0;
  n=hn-(size_t)(p-s);
  allocated=n/4*3+1;
  tmp = (unsigned char *)malloc (allocated);
  if (!tmp)
    return 0;
  if (!mulib_base64_decode (p, n, tmp, n / 4 * 3, &used) || used >= cap
      || !memchr (tmp, ':', used))
    {
      mulib_auth_wipe_(tmp,allocated);
      free (tmp);
      return 0;
    }
  for (size_t i = 0; i < used; ++i)
    if (tmp[i] < 32 || tmp[i] == 127)
      {
        mulib_auth_wipe_(tmp,allocated);
        free (tmp);
        return 0;
      }
  tmp[used] = 0;
  memcpy (out, tmp, used + 1);
  mulib_auth_wipe_(tmp,allocated);
  free (tmp);
  if (length)
    *length = used;
  return 1;
}
int
mulib_auth_basic_authorize (const char *s, mulib_auth_basic_verify_fn f, void *ctx)
{
  size_t hn=mulib_auth_length_(s), n=0;
  char *tmp, *colon;
  int ok=0;
  if (!hn || !f) return 0;
  tmp=(char *)malloc(hn+1);
  if (!tmp) return 0;
  if (mulib_auth_basic_n(s,hn,tmp,hn+1,&n)) {
    colon=(char *)memchr(tmp,':',n);
    ok=f(ctx,tmp,(size_t)(colon-tmp),colon+1,n-(size_t)(colon-tmp)-1)==1;
  }
  mulib_auth_wipe_(tmp,hn+1);
  free(tmp);
  return ok;
}
int
mulib_auth_bearer_authorize (const char *s, mulib_auth_token_verify_fn f, void *ctx)
{
  const char *token=mulib_auth_bearer(s);
  return token && f && f(ctx,token)==1;
}
int
mulib_auth_apikey (const void *expected, size_t en, const void *received, size_t rn)
{
  return expected && received && en && en<=MULIB_AUTH_MAX_HEADER && en==rn
         && mulib_security_equal(expected,received,en);
}
#endif
