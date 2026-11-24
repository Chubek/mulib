/** @file mulib-oauth.h
 * @brief OAuth2 convenience facade; not an OAuth 1.0 implementation.
 * Define MULIB_IMPL_OAUTH, OAUTH2, AUTH, AUTHCRYPTO, BASE64 and SECURITY. */
#ifndef MULIB_OAUTH_H
#define MULIB_OAUTH_H
#include "mulib-oauth2.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Delegate to OAuth2 bearer authorization. Missing verifier denies. */
int mulib_oauth_authorize (const char *authorization, const char *required_scope,
                           mulib_oauth2_verify_fn verify, void *context);
/** Compare already-decoded nonempty state; use oauth2_state_consume for replay
 * protection. */
int mulib_oauth_state (const char *expected, const char *received);
/** Compute an RFC 7636 S256 challenge. */
int mulib_oauth_pkce_challenge (const char *verifier, char *out, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OAUTH) && !defined(MULIB_OAUTH_IMPLEMENTED)
#define MULIB_OAUTH_IMPLEMENTED
int
mulib_oauth_authorize (const char *h, const char *scope, mulib_oauth2_verify_fn f, void *ctx)
{
  return mulib_oauth2_authorize(h,scope,f,ctx);
}
int
mulib_oauth_state (const char *a, const char *b)
{
  return mulib_oauth2_state(a,b);
}
int
mulib_oauth_pkce_challenge (const char *verifier, char *out, size_t cap)
{
  return mulib_oauth2_pkce_challenge(verifier,out,cap);
}
#endif
