/** @file mulib-hkdf.h
 * @brief HMAC/HKDF-SHA-256/384/512 and TLS 1.3 Expand-Label.
 * Define MULIB_IMPL_HKDF, SHA and ENTROPY independently. */
#ifndef MULIB_HKDF_H
#define MULIB_HKDF_H
#include "mulib-sha.h"
#include "mulib-entropy.h"
#ifdef __cplusplus
extern "C" {
#endif
/** SHA256, SHA384, SHA512 only. Output capacity >= digest size. Returns 1/0.
 * Output may overlap key/input. No allocation. */
int mulib_hmac (mulib_sha_algorithm, const void *key, size_t key_length,
                const void *data, size_t length, unsigned char *out, size_t capacity);
int mulib_hkdf_extract (mulib_sha_algorithm, const void *salt, size_t salt_length,
    const void *ikm, size_t ikm_length, unsigned char *prk, size_t capacity);
/** PRK must have at least hash-length bytes. Output cannot overlap inputs.
 * Length <= 255*hash-length. Invalid arguments leave output unchanged. */
int mulib_hkdf_expand (mulib_sha_algorithm, const void *prk, size_t prk_length,
    const void *info, size_t info_length, void *out, size_t length);
/** Prefixes the label with "tls13 "; context <=255, label <=249 bytes.
 * TLS helper uses SHA-256; secret is exactly 32 bytes. */
int mulib_hkdf_tls13 (const unsigned char secret[32], const char *label,
    const void *context, size_t context_length, void *out, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HKDF) && !defined(MULIB_HKDF_IMPLEMENTED)
#define MULIB_HKDF_IMPLEMENTED
#include <string.h>
static size_t mulib_hkdf_size_ (mulib_sha_algorithm a)
{ return a == MULIB_SHA256 ? 32 : a == MULIB_SHA384 ? 48 : a == MULIB_SHA512 ? 64 : 0; }
static void mulib_hkdf_hinit_ (mulib_sha_ctx *inner, mulib_sha_ctx *outer,
    mulib_sha_algorithm a, const void *key, size_t n)
{
  unsigned char pad[128] = {0};
  size_t block = a == MULIB_SHA256 ? 64 : 128;
  if (n > block) (void)mulib_sha (a, key, n, pad, sizeof pad);
  else if (n) memcpy (pad, key, n);
  for (size_t i = 0; i < block; ++i) pad[i] ^= 0x36;
  (void)mulib_sha_init (inner, a); (void)mulib_sha_update (inner, pad, block);
  for (size_t i = 0; i < block; ++i) pad[i] ^= 0x36 ^ 0x5c;
  (void)mulib_sha_init (outer, a); (void)mulib_sha_update (outer, pad, block);
  mulib_secret_wipe (pad, sizeof pad);
}
static void mulib_hkdf_hfinal_ (mulib_sha_ctx *inner, mulib_sha_ctx *outer, unsigned char *out)
{
  unsigned char digest[64];
  size_t n = inner->digest_size;
  (void)mulib_sha_final (inner, digest, sizeof digest);
  (void)mulib_sha_update (outer, digest, n); (void)mulib_sha_final (outer, out, n);
  mulib_secret_wipe (digest, sizeof digest);
  mulib_secret_wipe (inner, sizeof *inner); mulib_secret_wipe (outer, sizeof *outer);
}
int mulib_hmac (mulib_sha_algorithm a, const void *key, size_t kn,
    const void *data, size_t n, unsigned char *out, size_t cap)
{
  mulib_sha_ctx inner, outer;
  size_t size = mulib_hkdf_size_ (a);
  if (!size || (!key && kn) || (!data && n) || !out || cap < size
      || kn > UINT64_MAX / 8 || n > UINT64_MAX / 8 - 128) return 0;
  mulib_hkdf_hinit_ (&inner, &outer, a, key, kn);
  (void)mulib_sha_update (&inner, data, n); mulib_hkdf_hfinal_ (&inner, &outer, out);
  return 1;
}
int mulib_hkdf_extract (mulib_sha_algorithm a, const void *salt, size_t sn,
    const void *ikm, size_t n, unsigned char *prk, size_t capacity)
{ return mulib_hmac (a, salt, sn, ikm, n, prk, capacity); }
int mulib_hkdf_expand (mulib_sha_algorithm a, const void *prk, size_t pn,
    const void *info, size_t in, void *output, size_t n)
{
  unsigned char t[64] = {0}, *out = (unsigned char *)output;
  mulib_sha_ctx bi, bo, inner, outer;
  size_t size = mulib_hkdf_size_ (a), offset = 0, previous = 0;
  unsigned counter = 1;
  if (!size || !prk || pn < size || pn > UINT64_MAX / 8 || (!info && in)
      || in > UINT64_MAX / 8 - 193 || (!out && n) || n > 255 * size) return 0;
  mulib_hkdf_hinit_ (&bi, &bo, a, prk, pn);
  while (offset < n) {
    unsigned char c = (unsigned char)counter++;
    size_t take = n - offset < size ? n - offset : size;
    inner = bi; outer = bo;
    (void)mulib_sha_update (&inner, t, previous);
    (void)mulib_sha_update (&inner, info, in); (void)mulib_sha_update (&inner, &c, 1);
    mulib_hkdf_hfinal_ (&inner, &outer, t);
    memcpy (out + offset, t, take); offset += take; previous = size;
  }
  mulib_secret_wipe (t, sizeof t); mulib_secret_wipe (&bi, sizeof bi); mulib_secret_wipe (&bo, sizeof bo);
  return 1;
}
int mulib_hkdf_tls13 (const unsigned char secret[32], const char *label,
    const void *context, size_t cn, void *out, size_t n)
{
  unsigned char info[514];
  size_t ln = 0, used;
  if (!secret || !label || (!context && cn) || cn > 255 || n > 8160) return 0;
  while (ln < 250 && label[ln]) ++ln;
  if (ln > 249) return 0;
  info[0] = (unsigned char)(n >> 8); info[1] = (unsigned char)n;
  info[2] = (unsigned char)(6 + ln); memcpy (info + 3, "tls13 ", 6);
  memcpy (info + 9, label, ln); used = 9 + ln;
  info[used++] = (unsigned char)cn;
  if (cn) memcpy (info + used, context, cn);
  return mulib_hkdf_expand (MULIB_SHA256, secret, 32, info, used + cn, out, n);
}
#endif
