/** @file mulib-secret-webauthn.h
 * @brief WebAuthn assertion authenticator-data and signing-input builders.
 * Define MULIB_IMPL_SECRET_WEBAUTHN once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_WEBAUTHN_H
#define MULIB_SECRET_WEBAUTHN_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* This is not a complete WebAuthn authenticator. ClientDataJSON validation,
 * origins, CTAP, credential selection and UV must occur in trusted host.
 * This accepts lowercase ASCII DNS-style RP IDs, not URLs or raw Unicode.
 * UP and UV are booleans supplied by that host; UP must be 1 here. Output
 * must not overlap inputs and is unchanged on failure. */
int mulib_swa_authdata(const mulib_scr_provider *p,const char *rp_id,uint32_t counter,int user_present,int user_verified,unsigned char out[37]);
int mulib_swa_sign_input(const unsigned char authdata[37],const unsigned char client_hash[32],unsigned char out[69]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_WEBAUTHN) && !defined(MULIB_SECRET_WEBAUTHN_IMPLEMENTED)
#define MULIB_SECRET_WEBAUTHN_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int
mulib_swa_authdata(const mulib_scr_provider *p, const char *rp, uint32_t count,
                   int up, int uv, unsigned char out[37])
{
  if (!rp || !out || up != 1 || (uv != 0 && uv != 1)) return MULIB_SC_INVALID;
  size_t n = 0, label = 0;
  for (; rp[n]; ++n) {
    unsigned c = (unsigned char)rp[n];
    if (n >= 253) return MULIB_SC_INVALID;
    if (c == '.') {
      if (!label || rp[n - 1] == '-') return MULIB_SC_INVALID;
      label = 0;
    } else {
      if (label == 63 || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                           || (c == '-' && label))) return MULIB_SC_INVALID;
      ++label;
    }
  }
  if (!label || rp[n - 1] == '-' || mulib_sc_overlap(rp, n + 1, out, 37))
    return MULIB_SC_INVALID;
  mulib_sc_view id = {(const unsigned char *)rp, n};
  unsigned char digest[32];
  int rc = mulib_scr_hash(p, MULIB_SCR_SHA256, id, digest, sizeof digest);
  if (rc) return rc;
  memcpy(out, digest, sizeof digest);
  out[32] = (unsigned char)(1 | (uv ? 4 : 0));
  for (unsigned i = 0; i < 4; ++i) out[36 - i] = (unsigned char)(count >> (8 * i));
  return MULIB_SC_OK;
}

int
mulib_swa_sign_input(const unsigned char authdata[37], const unsigned char client_hash[32],
                     unsigned char out[69])
{
  if (!authdata || !client_hash || !out || mulib_sc_overlap(authdata, 37, out, 69)
      || mulib_sc_overlap(client_hash, 32, out, 69)) return MULIB_SC_INVALID;
  memcpy(out, authdata, 37);
  memcpy(out + 37, client_hash, 32);
  return MULIB_SC_OK;
}
#ifdef __cplusplus
}
#endif
#endif
