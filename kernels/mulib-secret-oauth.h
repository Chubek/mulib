/** @file mulib-secret-oauth.h
 * @brief OAuth PKCE S256, random state and token lease decisions; no network side effects.
 * Define MULIB_IMPL_SECRET_OAUTH once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_OAUTH_H
#define MULIB_SECRET_OAUTH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-codec.h"
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Outputs hold 43 Base64url characters and a NUL, and must not overlap.
 * Invalid arguments leave buffers unchanged; operational errors clear them. */
int mulib_soauth_pkce(const mulib_scr_provider *p, char verifier[44], char challenge[44]);
/* Compute S256 from the exact 43–128 RFC 7636 verifier bytes (no NUL). */
int mulib_soauth_pkce_challenge(const mulib_scr_provider *p, mulib_sc_view verifier,
                                char challenge[44]);
int mulib_soauth_state(const mulib_scr_provider *p, char state[44]);
/* Expiry zero is unknown, not perpetual. Returns 1 if refresh is needed. */
int mulib_soauth_refresh_needed(uint64_t now,uint64_t expiry,uint64_t margin);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_OAUTH) && !defined(MULIB_SECRET_OAUTH_IMPLEMENTED)
#define MULIB_SECRET_OAUTH_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int
mulib_soauth_state(const mulib_scr_provider *p, char out[44])
{
  if (!out) return MULIB_SC_INVALID;
  unsigned char entropy[32];
  int rc = mulib_scr_random(p, entropy, sizeof entropy);
  if (!rc) {
    mulib_sc_view in = {entropy, sizeof entropy};
    size_t written;
    rc = mulib_scode_encode(MULIB_SCODE_BASE64URL, in, out, 44, &written);
  }
  mulib_sc_wipe(entropy, sizeof entropy);
  if (rc) mulib_sc_wipe(out, 44);
  return rc;
}

int
mulib_soauth_pkce_challenge(const mulib_scr_provider *p, mulib_sc_view verifier,
                            char challenge[44])
{
  if (!verifier.ptr || verifier.len < 43 || verifier.len > 128 || !challenge
      || mulib_sc_overlap(verifier.ptr, verifier.len, challenge, 44))
    return MULIB_SC_INVALID;
  for (size_t i = 0; i < verifier.len; ++i) {
    unsigned c = verifier.ptr[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
          || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~'))
      return MULIB_SC_INVALID;
  }
  unsigned char digest[32];
  int rc = mulib_scr_hash(p, MULIB_SCR_SHA256, verifier, digest, sizeof digest);
  if (!rc) {
    mulib_sc_view in = {digest, sizeof digest};
    size_t written;
    rc = mulib_scode_encode(MULIB_SCODE_BASE64URL, in, challenge, 44, &written);
  }
  mulib_sc_wipe(digest, sizeof digest);
  if (rc) mulib_sc_wipe(challenge, 44);
  return rc;
}

int
mulib_soauth_pkce(const mulib_scr_provider *p, char verifier[44], char challenge[44])
{
  if (!verifier || !challenge || mulib_sc_overlap(verifier, 44, challenge, 44))
    return MULIB_SC_INVALID;
  int rc = mulib_soauth_state(p, verifier);
  if (!rc) {
    mulib_sc_view in = {(const unsigned char *)verifier, 43};
    rc = mulib_soauth_pkce_challenge(p, in, challenge);
  }
  if (rc) {
    mulib_sc_wipe(verifier, 44);
    mulib_sc_wipe(challenge, 44);
  }
  return rc;
}

int
mulib_soauth_refresh_needed(uint64_t now, uint64_t expiry, uint64_t margin)
{
  return !expiry || now >= expiry || expiry - now <= margin;
}
#ifdef __cplusplus
}
#endif
#endif
