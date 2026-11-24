/** @file mulib-oauth2.h
 * @brief OAuth2 bearer policy, scope/claim validation, one-use state and S256 PKCE.
 * Not an authorization server, JWT implementation or network client.
 * Define MULIB_IMPL_OAUTH2, AUTH, AUTHCRYPTO, BASE64 and SECURITY. */
#ifndef MULIB_OAUTH2_H
#define MULIB_OAUTH2_H
#include "mulib-auth.h"
#include "mulib-authcrypto.h"
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
  /** Exact, case-sensitive RFC 6749 scope-token set containment. Both strings
   * must be nonempty single-space-separated lists, without quotes/backslashes. */
  int mulib_oauth2_scope (const char *granted, const char *required);
  /** RFC 7636 S256 only: verifier is 43..128 unreserved ASCII characters. Output
   * needs 44 bytes (43 base64url characters and NUL), unchanged on failure. */
  int mulib_oauth2_pkce_challenge (const char *verifier, char *out, size_t capacity);
  /** Constant-work comparison at equal challenge lengths; no plain downgrade. */
  int mulib_oauth2_pkce_verify (const char *verifier, const char *challenge);
  typedef struct mulib_oauth2_transaction {
    const char *state;             /**< Borrowed unpredictable session-bound state. */
    uint64_t expires_at;
    int consumed;                 /**< Initialize to zero. */
  } mulib_oauth2_transaction_t;
  /** Validate and consume state before exchanging the code. Caller must bind
   * transaction to session/provider/client/redirect URI, serialize updates and
   * persist consumption. now is explicit Unix seconds; expiry is exclusive. */
  int mulib_oauth2_state_consume (mulib_oauth2_transaction_t *transaction,
                                  const char *received, uint64_t now);
  typedef struct mulib_oauth2_claims {
    const char *issuer, *audience, *scope;
    uint64_t expires_at, not_before;
  } mulib_oauth2_claims_t;
  /** Validate ALREADY AUTHENTICATED access-token claims. This never verifies
   * signatures. Audience is one exact expected audience, not a JWT array parser.
   * No clock skew is applied; now must be >= not_before and < expires_at. */
  int mulib_oauth2_claims_check (const mulib_oauth2_claims_t *claims,
                                 const char *issuer, const char *audience,
                                 const char *required_scope, uint64_t now);
  /** Verifier must cryptographically authenticate the token (or introspect it
   * over authenticated TLS), reject unsupported algorithms/token types and
   * return trusted claims whose strings remain live until authorize returns. */
  typedef int (*mulib_oauth2_claims_verify_fn) (void *context, const char *token,
                                               mulib_oauth2_claims_t *claims);
  /** Verify authenticity AND enforce issuer/audience/time/scope locally. */
  int mulib_oauth2_authorize_claims (const char *authorization,
                                     const char *issuer, const char *audience,
                                     const char *required_scope, uint64_t now,
                                     mulib_oauth2_claims_verify_fn verify,
                                     void *context);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OAUTH2) && !defined(MULIB_OAUTH2_IMPLEMENTED)
#define MULIB_OAUTH2_IMPLEMENTED
#include <string.h>
static size_t
mulib_oauth2_length_ (const char *s, size_t maximum)
{
  size_t n=0;
  if (!s) return 0;
  while (n<=maximum && s[n]) ++n;
  return n<=maximum ? n : 0;
}
static int
mulib_oauth2_scopes_ (const char *s)
{
  size_t n=mulib_oauth2_length_(s,MULIB_AUTH_MAX_HEADER);
  if (!n || s[0]==' ' || s[n-1]==' ') return 0;
  for (size_t i=0; i<n; ++i) {
    unsigned c=(unsigned char)s[i];
    if (c==' ') { if (s[i+1]==' ') return 0; }
    else if (c<0x21 || c>0x7e || c=='"' || c=='\\') return 0;
  }
  return 1;
}
int
mulib_oauth2_state (const char *a, const char *b)
{
  size_t n=mulib_oauth2_length_(a,MULIB_AUTH_MAX_HEADER);
  return n && n==mulib_oauth2_length_(b,MULIB_AUTH_MAX_HEADER)
         && mulib_security_equal (a, b, n);
}
int
mulib_oauth2_authorize (const char *h, const char *scope,
                        mulib_oauth2_verify_fn f, void *ctx)
{
  const char *token = mulib_auth_bearer (h);
  return token && mulib_oauth2_scopes_(scope) && f && f (ctx, token, scope) == 1;
}
int
mulib_oauth2_scope (const char *granted, const char *required)
{
  const char *r, *g;
  if (!mulib_oauth2_scopes_(granted) || !mulib_oauth2_scopes_(required)) return 0;
  for (r=required; *r;) {
    size_t rn=strcspn(r," ");
    int found=0;
    for (g=granted; *g;) {
      size_t gn=strcspn(g," ");
      if (gn==rn && !memcmp(r,g,rn)) found=1;
      g+=gn; if (*g) ++g;
    }
    if (!found) return 0;
    r+=rn; if (*r) ++r;
  }
  return 1;
}
int
mulib_oauth2_pkce_challenge (const char *verifier, char *out, size_t cap)
{
  unsigned char digest[32];
  char text[45];
  size_t n=mulib_oauth2_length_(verifier,128);
  if (n<43 || !out || cap<44) return 0;
  for (size_t i=0; i<n; ++i) {
    unsigned c=(unsigned char)verifier[i];
    if (!((c>='A' && c<='Z') || (c>='a' && c<='z')
          || (c>='0' && c<='9') || strchr("-._~",(int)c))) return 0;
  }
  if (!mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA256,verifier,n,digest,sizeof digest)) return 0;
  (void)mulib_base64_encode(digest,sizeof digest,text,sizeof text);
  for (unsigned i=0; i<43; ++i) {
    if (text[i]=='+') text[i]='-';
    else if (text[i]=='/') text[i]='_';
  }
  text[43]=0;
  memcpy(out,text,44);
  mulib_authcrypto_wipe(digest,sizeof digest);
  return 1;
}
int
mulib_oauth2_pkce_verify (const char *verifier, const char *challenge)
{
  char expected[44];
  return mulib_oauth2_length_(challenge,43)==43
         && mulib_oauth2_pkce_challenge(verifier,expected,sizeof expected)
         && mulib_security_equal(expected,challenge,43);
}
int
mulib_oauth2_state_consume (mulib_oauth2_transaction_t *t, const char *received,
                            uint64_t now)
{
  if (!t || t->consumed!=0 || now>=t->expires_at
      || !mulib_oauth2_state(t->state,received)) return 0;
  t->consumed=1;
  return 1;
}
int
mulib_oauth2_claims_check (const mulib_oauth2_claims_t *c, const char *issuer,
                           const char *audience, const char *scope, uint64_t now)
{
  return c && mulib_oauth2_length_(issuer,MULIB_AUTH_MAX_HEADER)
         && mulib_oauth2_length_(audience,MULIB_AUTH_MAX_HEADER)
         && mulib_oauth2_state(issuer,c->issuer) && mulib_oauth2_state(audience,c->audience)
         && now<c->expires_at && now>=c->not_before
         && mulib_oauth2_scope(c->scope,scope);
}
int
mulib_oauth2_authorize_claims (const char *h, const char *issuer, const char *audience,
                               const char *scope, uint64_t now,
                               mulib_oauth2_claims_verify_fn f, void *ctx)
{
  const char *token=mulib_auth_bearer(h);
  mulib_oauth2_claims_t claims;
  memset(&claims,0,sizeof claims);
  if (!token || !f || !mulib_oauth2_length_(issuer,MULIB_AUTH_MAX_HEADER)
      || !mulib_oauth2_length_(audience,MULIB_AUTH_MAX_HEADER)
      || !mulib_oauth2_scopes_(scope) || f(ctx,token,&claims)!=1) return 0;
  return mulib_oauth2_claims_check(&claims,issuer,audience,scope,now);
}
#endif
