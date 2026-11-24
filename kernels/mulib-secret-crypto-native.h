/** @file mulib-secret-crypto-native.h
 * @brief Adapter for existing MuLIB crypto primitives; no new external dependency.
 * Define MULIB_IMPL_SECRET_CRYPTO_NATIVE once. Also enable implementations of
 * SECRET_CRYPTO, SECRET_CORE, ENTROPY, AUTHCRYPTO, SHA, HKDF and AES_GCM separately.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_CRYPTO_NATIVE_H
#define MULIB_SECRET_CRYPTO_NATIVE_H
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* AES-128-GCM, SHA1/256/384/512, HMAC and PBKDF2. No Argon2 or stronger AEAD fallback. */
void mulib_scr_native(mulib_scr_provider *out);
/* Nominal capability of the unmodified built-in provider, not host policy. */
int mulib_scr_native_supports(mulib_scr_operation op, unsigned algorithm);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_CRYPTO_NATIVE) && !defined(MULIB_SECRET_CRYPTO_NATIVE_IMPLEMENTED)
#define MULIB_SECRET_CRYPTO_NATIVE_IMPLEMENTED
#include "mulib-entropy.h"
#include "mulib-authcrypto.h"
#include "mulib-sha.h"
#include "mulib-hkdf.h"
#include "mulib-aes-gcm.h"
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_scr_native_alg_(unsigned a, mulib_sha_algorithm *out)
{
  switch (a) {
  case MULIB_SCR_SHA1: *out = MULIB_SHA1; return 1;
  case MULIB_SCR_SHA256: *out = MULIB_SHA256; return 1;
  case MULIB_SCR_SHA384: *out = MULIB_SHA384; return 1;
  case MULIB_SCR_SHA512: *out = MULIB_SHA512; return 1;
  default: return 0;
  }
}

int mulib_scr_native_supports(mulib_scr_operation op, unsigned a)
{
  mulib_sha_algorithm sha;
  switch (op) {
  case MULIB_SCR_OP_RANDOM:
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    return a == 0;
#else
    return 0;
#endif
  case MULIB_SCR_OP_HASH:
  case MULIB_SCR_OP_HMAC: return mulib_scr_native_alg_(a, &sha);
  case MULIB_SCR_OP_HKDF: return a != MULIB_SCR_SHA1 && mulib_scr_native_alg_(a, &sha);
  case MULIB_SCR_OP_KDF: return a == MULIB_SCR_PBKDF2_SHA256;
  case MULIB_SCR_OP_SEAL:
  case MULIB_SCR_OP_OPEN: return a == MULIB_SCR_AES128_GCM;
  default: return 0;
  }
}

static int mulib_scr_native_rng_(void *ctx, unsigned char *out, size_t n)
{
  (void)ctx;
  if (!mulib_scr_native_supports(MULIB_SCR_OP_RANDOM, 0)) return MULIB_SC_UNSUPPORTED;
  return mulib_entropy(out, n) ? MULIB_SC_OK : MULIB_SC_IO;
}

static int mulib_scr_native_hash_(void *ctx, unsigned a, mulib_sc_view in,
                                  unsigned char *out, size_t n)
{
  mulib_sha_algorithm sha;
  (void)ctx;
  if (!mulib_scr_native_alg_(a, &sha)) return MULIB_SC_UNSUPPORTED;
  return mulib_sha(sha, in.ptr, in.len, out, n) ? MULIB_SC_OK : MULIB_SC_INVALID;
}

static int mulib_scr_native_hmac_(void *ctx, unsigned a, mulib_sc_view key,
                                  mulib_sc_view in, unsigned char *out, size_t n)
{
  mulib_sha_algorithm sha;
  (void)ctx;
  if (!mulib_scr_native_alg_(a, &sha)) return MULIB_SC_UNSUPPORTED;
  /* MuLIB's SHA-family HMAC does not provide legacy SHA-1. */
  int ok = a == MULIB_SCR_SHA1
    ? mulib_authcrypto_hmac(MULIB_AUTHCRYPTO_SHA1, key.ptr, key.len, in.ptr, in.len, out, n)
    : mulib_hmac(sha, key.ptr, key.len, in.ptr, in.len, out, n);
  return ok ? MULIB_SC_OK : MULIB_SC_INVALID;
}

static int mulib_scr_native_kdf_(void *ctx, mulib_scr_kdf_params q, mulib_sc_view pass,
                                 mulib_sc_view salt, unsigned char out[32])
{
  (void)ctx;
  if (!mulib_scr_native_supports(MULIB_SCR_OP_KDF, q.algorithm)) return MULIB_SC_UNSUPPORTED;
  return mulib_authcrypto_pbkdf2(pass.ptr, pass.len, salt.ptr, salt.len, q.iterations, out)
    ? MULIB_SC_OK : MULIB_SC_INVALID;
}

static int mulib_scr_native_seal_(void *ctx, unsigned a, mulib_sc_view key, mulib_sc_view nonce,
                                  mulib_sc_view aad, mulib_sc_view in, unsigned char *out,
                                  unsigned char tag[16])
{
  mulib_aes_gcm_t aes;
  (void)ctx;
  if (!mulib_scr_native_supports(MULIB_SCR_OP_SEAL, a)) return MULIB_SC_UNSUPPORTED;
  if (!mulib_aes_gcm_init(&aes, key.ptr)) return MULIB_SC_INVALID;
  int ok = mulib_aes_gcm_seal(&aes, nonce.ptr, aad.ptr, aad.len, in.ptr, in.len, out, tag);
  mulib_aes_gcm_clear(&aes);
  return ok ? MULIB_SC_OK : MULIB_SC_INVALID;
}

static int mulib_scr_native_open_(void *ctx, unsigned a, mulib_sc_view key, mulib_sc_view nonce,
                                  mulib_sc_view aad, mulib_sc_view in,
                                  const unsigned char tag[16], unsigned char *out)
{
  mulib_aes_gcm_t aes;
  (void)ctx;
  if (!mulib_scr_native_supports(MULIB_SCR_OP_OPEN, a)) return MULIB_SC_UNSUPPORTED;
  if (!mulib_aes_gcm_init(&aes, key.ptr)) return MULIB_SC_INVALID;
  int ok = mulib_aes_gcm_open(&aes, nonce.ptr, aad.ptr, aad.len, in.ptr, in.len, tag, out);
  mulib_aes_gcm_clear(&aes);
  return ok ? MULIB_SC_OK : MULIB_SC_AUTH;
}

void mulib_scr_native(mulib_scr_provider *out)
{
  const mulib_scr_provider p = {
    MULIB_SCR_ABI_V1, NULL, mulib_scr_native_rng_, mulib_scr_native_hash_,
    mulib_scr_native_hmac_, mulib_scr_native_kdf_, mulib_scr_native_seal_, mulib_scr_native_open_
  };
  if (out) *out = p;
}
#ifdef __cplusplus
}
#endif
#endif
