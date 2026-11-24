/** @file mulib-secret-key-agreement.h
 * @brief Native X25519 key generation and domain-separated shared-key derivation.
 * Define MULIB_IMPL_SECRET_KEY_AGREEMENT once, with separate SECRET_CRYPTO,
 * SECRET_CORE, X25519 and ENTROPY implementations.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_KEY_AGREEMENT_H
#define MULIB_SECRET_KEY_AGREEMENT_H
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Private scalar is random bytes; MuLIB X25519 clamps internally. Derive
 * produces HKDF-SHA256 output, not raw Diffie-Hellman. The host MUST enforce
 * peer authentication and supply a nonempty canonical transcript that binds
 * the protocol, roles, and both identities and keys; this is not a handshake.
 * Outputs must be disjoint from inputs, one another and the provider.
 * On argument error outputs are unchanged; after validation failures wipe
 * each 32-byte output. */
int mulib_ska_generate(const mulib_scr_provider *p, unsigned char private_key[32],
                        unsigned char public_key[32]);
int mulib_ska_derive(const mulib_scr_provider *p, const unsigned char private_key[32],
                      const unsigned char peer_key[32], mulib_sc_view transcript,
                      unsigned char session_key[32]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_KEY_AGREEMENT) && !defined(MULIB_SECRET_KEY_AGREEMENT_IMPLEMENTED)
#define MULIB_SECRET_KEY_AGREEMENT_IMPLEMENTED
#include "mulib-x25519.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ska_generate(const mulib_scr_provider *p, unsigned char s[32], unsigned char pub[32])
{
  if (!s || !pub || mulib_sc_overlap(s, 32, pub, 32)
      || mulib_sc_overlap(s, 32, p, sizeof *p) || mulib_sc_overlap(pub, 32, p, sizeof *p))
    return MULIB_SC_INVALID;
  int r = mulib_scr_random(p, s, 32);
  if (!r && !mulib_x25519_public(pub, s)) r = MULIB_SC_AUTH;
  if (r) {
    mulib_sc_wipe(s, 32);
    mulib_sc_wipe(pub, 32);
  }
  return r;
}

int mulib_ska_derive(const mulib_scr_provider *p, const unsigned char s[32],
                      const unsigned char peer[32], mulib_sc_view tr, unsigned char out[32])
{
  static const unsigned char label[] = "MuLIB.X25519.v1";
  unsigned char raw[32], salt[32];
  if (!s || !peer || !out || !tr.ptr || !tr.len || tr.len > UINT32_C(1048576)
      || mulib_sc_overlap(out, 32, p, sizeof *p) || mulib_sc_overlap(out, 32, s, 32)
      || mulib_sc_overlap(out, 32, peer, 32) || mulib_sc_overlap(out, 32, tr.ptr, tr.len))
    return MULIB_SC_INVALID;
  int r = mulib_x25519(raw, s, peer) ? MULIB_SC_OK : MULIB_SC_AUTH;
  if (!r) r = mulib_scr_hash(p, MULIB_SCR_SHA256, tr, salt, sizeof salt);
  if (!r) {
    mulib_sc_view ikm = {raw, sizeof raw}, sv = {salt, sizeof salt};
    mulib_sc_view info = {label, sizeof label - 1};
    r = mulib_scr_hkdf(p, MULIB_SCR_SHA256, sv, ikm, info, out, 32);
  }
  mulib_sc_wipe(raw, sizeof raw);
  mulib_sc_wipe(salt, sizeof salt);
  if (r) mulib_sc_wipe(out, 32);
  return r;
}
#ifdef __cplusplus
}
#endif
#endif
