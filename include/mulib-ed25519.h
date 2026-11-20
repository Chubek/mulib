/** @file mulib-ed25519.h
 * @brief Pure Ed25519 via OpenSSL EVP. Define MULIB_IMPL_ED25519 and link libcrypto.
 */
#ifndef MULIB_ED25519_H
#define MULIB_ED25519_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Private keys are 32-byte RFC 8032 seeds. Return 1 on success, 0 on
 * invalid arguments or provider failure. Outputs are committed on success. */
int mulib_ed25519_public (unsigned char public_key[32], const unsigned char seed[32]);
int mulib_ed25519_sign (unsigned char signature[64], const unsigned char seed[32],
                       const void *message, size_t length);
int mulib_ed25519_verify (const unsigned char signature[64], const unsigned char public_key[32],
                         const void *message, size_t length);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ED25519) && !defined(MULIB_ED25519_IMPLEMENTED)
#define MULIB_ED25519_IMPLEMENTED
#include <openssl/evp.h>
#include <string.h>

int
mulib_ed25519_public (unsigned char public_key[32], const unsigned char seed[32])
{
  unsigned char raw[32];
  size_t length = sizeof raw;
  if (!public_key || !seed) return 0;
  EVP_PKEY *key = EVP_PKEY_new_raw_private_key (EVP_PKEY_ED25519, NULL, seed, 32);
  int ok = key && EVP_PKEY_get_raw_public_key (key, raw, &length) == 1
           && length == sizeof raw;
  EVP_PKEY_free (key);
  if (ok) memcpy (public_key, raw, sizeof raw);
  return ok;
}

int
mulib_ed25519_sign (unsigned char signature[64], const unsigned char seed[32],
                    const void *message, size_t length)
{
  unsigned char raw[64];
  size_t size = sizeof raw;
  if (!signature || !seed || (!message && length)) return 0;
  EVP_PKEY *key = EVP_PKEY_new_raw_private_key (EVP_PKEY_ED25519, NULL, seed, 32);
  EVP_MD_CTX *ctx = EVP_MD_CTX_new ();
  int ok = key && ctx && EVP_DigestSignInit (ctx, NULL, NULL, NULL, key) == 1
           && EVP_DigestSign (ctx, raw, &size, message ? message : "", length) == 1
           && size == sizeof raw;
  EVP_MD_CTX_free (ctx);
  EVP_PKEY_free (key);
  if (ok) memcpy (signature, raw, sizeof raw);
  return ok;
}

int
mulib_ed25519_verify (const unsigned char signature[64], const unsigned char public_key[32],
                      const void *message, size_t length)
{
  if (!signature || !public_key || (!message && length)) return 0;
  EVP_PKEY *key = EVP_PKEY_new_raw_public_key (EVP_PKEY_ED25519, NULL, public_key, 32);
  EVP_MD_CTX *ctx = EVP_MD_CTX_new ();
  int ok = key && ctx && EVP_DigestVerifyInit (ctx, NULL, NULL, NULL, key) == 1
           && EVP_DigestVerify (ctx, signature, 64, message ? message : "", length) == 1;
  EVP_MD_CTX_free (ctx);
  EVP_PKEY_free (key);
  return ok;
}
#endif
