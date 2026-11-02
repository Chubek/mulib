/** @file mulib-oauth2.h
 * @brief OAuth2 callback state checking and explicit bearer-verifier adapter.
 * Not an authorization server, JWT implementation or network client.
 * Define MULIB_IMPL_OAUTH2, AUTH and its dependencies. */
#ifndef MULIB_OAUTH2_H
#define MULIB_OAUTH2_H
#include "mulib-auth.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_oauth2_verify_fn) (void *context, const char *token,
                                         const char *required_scope);
  /** Verifier must check signature/introspection, issuer, audience, expiry and
   * scope. Missing verifier, token or scope fails closed. */
  int mulib_oauth2_authorize (const char *authorization, const char *scope,
                              mulib_oauth2_verify_fn verify, void *context);
  /** Expected state must be application-generated, unpredictable, session-bound
   * and single-use. This helper compares nonempty already-decoded values only.
   */
  int mulib_oauth2_state (const char *expected, const char *received);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OAUTH2) && !defined(MULIB_OAUTH2_IMPLEMENTED)
#define MULIB_OAUTH2_IMPLEMENTED
#include <string.h>
int
mulib_oauth2_state (const char *a, const char *b)
{
  return a && b && *a && strlen (a) == strlen (b)
         && mulib_security_equal (a, b, strlen (a));
}
int
mulib_oauth2_authorize (const char *h, const char *scope,
                        mulib_oauth2_verify_fn f, void *ctx)
{
  const char *token = mulib_auth_bearer (h);
  return token && scope && *scope && f && f (ctx, token, scope) == 1;
}
#endif
