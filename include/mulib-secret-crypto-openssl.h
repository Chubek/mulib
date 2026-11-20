/** @file mulib-secret-crypto-openssl.h
 * @brief Optional OpenSSL EVP provider for differential testing and deployments allowing external crypto.
 * Define MULIB_IMPL_SECRET_CRYPTO_OPENSSL once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_CRYPTO_OPENSSL_H
#define MULIB_SECRET_CRYPTO_OPENSSL_H
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional, NOT required by MuLIB native backend. Only implementation TU
 * needs OpenSSL headers/libcrypto. Argon2id and XChaCha remain unsupported. */
void mulib_scr_openssl(mulib_scr_provider *out);
/* Nominal capability, not a check of the active OpenSSL provider configuration. */
int mulib_scr_openssl_supports(mulib_scr_operation op, unsigned algorithm);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_CRYPTO_OPENSSL) && !defined(MULIB_SECRET_CRYPTO_OPENSSL_IMPLEMENTED)
#define MULIB_SECRET_CRYPTO_OPENSSL_IMPLEMENTED
#include <limits.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#ifdef __cplusplus
extern "C" {
#endif
static const EVP_MD *mulib_sossl_md_(unsigned a)
{
  switch (a) {
  case MULIB_SCR_SHA1: return EVP_sha1();
  case MULIB_SCR_SHA256: return EVP_sha256();
  case MULIB_SCR_SHA384: return EVP_sha384();
  case MULIB_SCR_SHA512: return EVP_sha512();
  default: return NULL;
  }
}

static const EVP_CIPHER *mulib_sossl_cipher_(unsigned a)
{
  switch (a) {
  case MULIB_SCR_AES128_GCM: return EVP_aes_128_gcm();
  case MULIB_SCR_AES256_GCM: return EVP_aes_256_gcm();
  default: return NULL;
  }
}

int mulib_scr_openssl_supports(mulib_scr_operation op, unsigned a)
{
  switch (op) {
  case MULIB_SCR_OP_RANDOM: return a == 0;
  case MULIB_SCR_OP_HASH:
  case MULIB_SCR_OP_HMAC: return mulib_sossl_md_(a) != NULL;
  case MULIB_SCR_OP_HKDF: return a != MULIB_SCR_SHA1 && mulib_sossl_md_(a) != NULL;
  case MULIB_SCR_OP_KDF: return a == MULIB_SCR_PBKDF2_SHA256;
  case MULIB_SCR_OP_SEAL:
  case MULIB_SCR_OP_OPEN: return mulib_sossl_cipher_(a) != NULL;
  default: return 0;
  }
}

static int mulib_sossl_random_(void *ctx, unsigned char *out, size_t n)
{
  (void)ctx;
  while (n) {
    int take = n > INT_MAX ? INT_MAX : (int)n;
    if (RAND_bytes(out, take) != 1) return MULIB_SC_IO;
    out += take;
    n -= (size_t)take;
  }
  return MULIB_SC_OK;
}

static const unsigned char *mulib_sossl_nonnull_(mulib_sc_view v)
{
  static const unsigned char empty = 0;
  return v.ptr ? v.ptr : &empty;
}

static int mulib_sossl_hash_(void *ctx, unsigned a, mulib_sc_view in,
                             unsigned char *out, size_t n)
{
  const EVP_MD *md = mulib_sossl_md_(a);
  unsigned written = 0;
  (void)ctx;
  if (!md) return MULIB_SC_UNSUPPORTED;
  if (n != mulib_scr_digest_size(a)) return MULIB_SC_INVALID;
  return EVP_Digest(mulib_sossl_nonnull_(in), in.len, out, &written, md, NULL) == 1
    && written == n ? MULIB_SC_OK : MULIB_SC_IO;
}

static int mulib_sossl_hmac_(void *ctx, unsigned a, mulib_sc_view key,
                             mulib_sc_view in, unsigned char *out, size_t n)
{
  const EVP_MD *md = mulib_sossl_md_(a);
  unsigned written = 0;
  (void)ctx;
  if (!md) return MULIB_SC_UNSUPPORTED;
  if (n != mulib_scr_digest_size(a)) return MULIB_SC_INVALID;
  if (key.len > INT_MAX) return MULIB_SC_LIMIT;
  return HMAC(md, mulib_sossl_nonnull_(key), (int)key.len,
              mulib_sossl_nonnull_(in), in.len, out, &written)
    && written == n ? MULIB_SC_OK : MULIB_SC_IO;
}

static int mulib_sossl_kdf_(void *ctx, mulib_scr_kdf_params q, mulib_sc_view pass,
                            mulib_sc_view salt, unsigned char out[32])
{
  (void)ctx;
  if (!mulib_scr_openssl_supports(MULIB_SCR_OP_KDF, q.algorithm)) return MULIB_SC_UNSUPPORTED;
  if (pass.len > INT_MAX || salt.len > INT_MAX || q.iterations > INT_MAX) return MULIB_SC_LIMIT;
  return PKCS5_PBKDF2_HMAC((const char *)pass.ptr, (int)pass.len, salt.ptr, (int)salt.len,
                           (int)q.iterations, EVP_sha256(), 32, out) == 1
    ? MULIB_SC_OK : MULIB_SC_IO;
}

static int mulib_sossl_seal_(void *ctx, unsigned a, mulib_sc_view key, mulib_sc_view nonce,
                             mulib_sc_view aad, mulib_sc_view in, unsigned char *out,
                             unsigned char tag[16])
{
  const EVP_CIPHER *cipher = mulib_sossl_cipher_(a);
  unsigned char tail[EVP_MAX_BLOCK_LENGTH];
  int z = 0, written = 0;
  (void)ctx;
  if (!cipher) return MULIB_SC_UNSUPPORTED;
  if (in.len > INT_MAX || aad.len > INT_MAX) return MULIB_SC_LIMIT;
  EVP_CIPHER_CTX *x = EVP_CIPHER_CTX_new();
  if (!x) return MULIB_SC_NOMEM;
  int ok = EVP_EncryptInit_ex(x, cipher, NULL, NULL, NULL) == 1
    && EVP_CIPHER_CTX_block_size(x) == 1 /* Update may write directly to an exact-size buffer. */
    && EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1
    && EVP_EncryptInit_ex(x, NULL, NULL, key.ptr, nonce.ptr) == 1
    && (!aad.len || EVP_EncryptUpdate(x, NULL, &z, aad.ptr, (int)aad.len) == 1)
    && (!in.len || EVP_EncryptUpdate(x, out, &written, in.ptr, (int)in.len) == 1)
    && written == (int)in.len && EVP_EncryptFinal_ex(x, tail, &z) == 1 && z == 0
    && EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1;
  mulib_sc_wipe(tail, sizeof tail);
  EVP_CIPHER_CTX_free(x);
  return ok ? MULIB_SC_OK : MULIB_SC_IO;
}

static int mulib_sossl_open_(void *ctx, unsigned a, mulib_sc_view key, mulib_sc_view nonce,
                             mulib_sc_view aad, mulib_sc_view in,
                             const unsigned char tag[16], unsigned char *out)
{
  const EVP_CIPHER *cipher = mulib_sossl_cipher_(a);
  unsigned char tail[EVP_MAX_BLOCK_LENGTH], copy[16];
  int z = 0, written = 0, status = MULIB_SC_IO;
  (void)ctx;
  if (!cipher) return MULIB_SC_UNSUPPORTED;
  if (in.len > INT_MAX || aad.len > INT_MAX) return MULIB_SC_LIMIT;
  EVP_CIPHER_CTX *x = EVP_CIPHER_CTX_new();
  if (!x) return MULIB_SC_NOMEM;
  /* EVP may produce plaintext during Update. Keep it private until Final. */
  unsigned char *tmp = in.len ? (unsigned char *)OPENSSL_malloc(in.len) : NULL;
  if (in.len && !tmp) {
    EVP_CIPHER_CTX_free(x);
    return MULIB_SC_NOMEM;
  }
  memcpy(copy, tag, sizeof copy);
  if (EVP_DecryptInit_ex(x, cipher, NULL, NULL, NULL) == 1
      && EVP_CIPHER_CTX_block_size(x) == 1
      && EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1
      && EVP_DecryptInit_ex(x, NULL, NULL, key.ptr, nonce.ptr) == 1
      && (!aad.len || EVP_DecryptUpdate(x, NULL, &z, aad.ptr, (int)aad.len) == 1)
      && (!in.len || EVP_DecryptUpdate(x, tmp, &written, in.ptr, (int)in.len) == 1)
      && written == (int)in.len
      && EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_AEAD_SET_TAG, 16, copy) == 1) {
    if (EVP_DecryptFinal_ex(x, tail, &z) != 1) {
      status = MULIB_SC_AUTH;
    } else if (z == 0) {
      if (in.len) memcpy(out, tmp, in.len);
      status = MULIB_SC_OK;
    }
  }
  OPENSSL_clear_free(tmp, in.len);
  mulib_sc_wipe(tail, sizeof tail);
  mulib_sc_wipe(copy, sizeof copy);
  EVP_CIPHER_CTX_free(x);
  return status;
}

void mulib_scr_openssl(mulib_scr_provider *out)
{
  const mulib_scr_provider p = {
    MULIB_SCR_ABI_V1, NULL, mulib_sossl_random_, mulib_sossl_hash_,
    mulib_sossl_hmac_, mulib_sossl_kdf_, mulib_sossl_seal_, mulib_sossl_open_
  };
  if (out) *out = p;
}
#ifdef __cplusplus
}
#endif
#endif
