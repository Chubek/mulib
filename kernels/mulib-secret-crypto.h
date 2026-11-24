/** @file mulib-secret-crypto.h
 * @brief Versioned crypto provider ABI, checked RNG, HMAC, HKDF, KDF and AEAD dispatch.
 * Define MULIB_IMPL_SECRET_CRYPTO once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_CRYPTO_H
#define MULIB_SECRET_CRYPTO_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-core.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_SCR_SHA1=1,MULIB_SCR_SHA256=2,MULIB_SCR_SHA512=3,MULIB_SCR_SHA384=4 };
enum { MULIB_SCR_AES128_GCM=1,MULIB_SCR_AES256_GCM=2,MULIB_SCR_XCHACHA20_POLY1305=3 };
enum { MULIB_SCR_PBKDF2_SHA256=1,MULIB_SCR_ARGON2ID=2 };
enum { MULIB_SCR_ABI_V1 = 1 };
/* Operations for built-in backend capability queries. Use algorithm 0 for RNG. */
typedef enum {
  MULIB_SCR_OP_RANDOM, MULIB_SCR_OP_HASH, MULIB_SCR_OP_HMAC,
  MULIB_SCR_OP_HKDF, MULIB_SCR_OP_KDF, MULIB_SCR_OP_SEAL, MULIB_SCR_OP_OPEN
} mulib_scr_operation;
typedef struct { uint32_t algorithm,iterations,memory_kib,lanes; } mulib_scr_kdf_params;
/* Trusted v1 callbacks are called through the checked functions below with
 * immutable provider settings and validated arguments. Return SC status, not
 * MuLIB legacy boolean, and write exact lengths on success. AEAD open MUST
 * authenticate before releasing plaintext. Zero-length buffers may be NULL. */
typedef struct {
  uint32_t abi;
  void *ctx;
  int (*random)(void *, unsigned char *, size_t);
  int (*hash)(void *, unsigned, mulib_sc_view, unsigned char *, size_t);
  int (*hmac)(void *, unsigned, mulib_sc_view, mulib_sc_view, unsigned char *, size_t);
  int (*kdf)(void *, mulib_scr_kdf_params, mulib_sc_view, mulib_sc_view, unsigned char[32]);
  int (*seal)(void *, unsigned, mulib_sc_view, mulib_sc_view, mulib_sc_view, mulib_sc_view,
              unsigned char *, unsigned char[16]);
  int (*open)(void *, unsigned, mulib_sc_view, mulib_sc_view, mulib_sc_view, mulib_sc_view,
              const unsigned char[16], unsigned char *);
} mulib_scr_provider;
size_t mulib_scr_digest_size(unsigned alg);
size_t mulib_scr_key_size(unsigned alg);
size_t mulib_scr_nonce_size(unsigned alg);
/* Callers supply actual valid buffers. Written ranges must not overlap inputs,
 * each other, or the provider and its context; this API does not support
 * in-place AEAD. The caller must use a unique nonce for each seal with the
 * same key.
 *
 * Argument-validation failures leave outputs unchanged. After validation,
 * operation failure (including a missing callback) wipes only the requested
 * output ranges: n bytes, digest size, 32 bytes for KDF, or input length plus
 * the seal tag for AEAD. Extra capacity is never written. Outputs are usable
 * only on MULIB_SC_OK. Callback results outside SC status map to MULIB_SC_IO.
 * For seal/open, both input and AAD are limited to 64 MiB. */
int mulib_scr_random(const mulib_scr_provider *p,void *out,size_t n);
int mulib_scr_hash(const mulib_scr_provider *p,unsigned alg,mulib_sc_view in,unsigned char *out,size_t cap);
int mulib_scr_hmac(const mulib_scr_provider *p,unsigned alg,mulib_sc_view key,mulib_sc_view in,unsigned char *out,size_t cap);
/* SHA-256/384/512 only. Info length <= 1024; n <= 255 times the digest size. */
int mulib_scr_hkdf(const mulib_scr_provider *p,unsigned alg,mulib_sc_view salt,mulib_sc_view ikm,mulib_sc_view info,unsigned char *out,size_t n);
/* Bounded password profiles only. Unknown profile IDs are argument errors
 * reported as MULIB_SC_UNSUPPORTED without dispatching or writing output. */
int mulib_scr_kdf(const mulib_scr_provider *p,mulib_scr_kdf_params params,mulib_sc_view pass,mulib_sc_view salt,unsigned char out[32]);
int mulib_scr_seal(const mulib_scr_provider *p,unsigned alg,mulib_sc_view key,mulib_sc_view nonce,mulib_sc_view aad,mulib_sc_view plain,mulib_sc_buf out,unsigned char tag[16]);
int mulib_scr_open(const mulib_scr_provider *p,unsigned alg,mulib_sc_view key,mulib_sc_view nonce,mulib_sc_view aad,mulib_sc_view cipher,const unsigned char tag[16],mulib_sc_buf out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_CRYPTO) && !defined(MULIB_SECRET_CRYPTO_IMPLEMENTED)
#define MULIB_SECRET_CRYPTO_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_scr_digest_size(unsigned a)
{
  switch (a) {
  case MULIB_SCR_SHA1: return 20;
  case MULIB_SCR_SHA256: return 32;
  case MULIB_SCR_SHA384: return 48;
  case MULIB_SCR_SHA512: return 64;
  default: return 0;
  }
}

size_t mulib_scr_key_size(unsigned a)
{
  switch (a) {
  case MULIB_SCR_AES128_GCM: return 16;
  case MULIB_SCR_AES256_GCM:
  case MULIB_SCR_XCHACHA20_POLY1305: return 32;
  default: return 0;
  }
}

size_t mulib_scr_nonce_size(unsigned a)
{
  switch (a) {
  case MULIB_SCR_AES128_GCM:
  case MULIB_SCR_AES256_GCM: return 12;
  case MULIB_SCR_XCHACHA20_POLY1305: return 24;
  default: return 0;
  }
}

static int mulib_scr_valid_(const mulib_scr_provider *p)
{
  return p && p->abi == MULIB_SCR_ABI_V1;
}

static int mulib_scr_alias_(const void *out, size_t n, mulib_sc_view v)
{
  return mulib_sc_overlap(out, n, v.ptr, v.len);
}

static int mulib_scr_output_(const mulib_scr_provider *p, const void *out, size_t n)
{
  return (out || !n) && !mulib_sc_overlap(out, n, p, sizeof *p);
}

static int mulib_scr_finish_(int status, void *out, size_t n)
{
  if (status != MULIB_SC_OK) mulib_sc_wipe(out, n);
  if (status > MULIB_SC_OK || status < MULIB_SC_NOTFOUND) return MULIB_SC_IO;
  return status;
}

int mulib_scr_random(const mulib_scr_provider *p, void *out, size_t n)
{
  if (!mulib_scr_output_(p, out, n)) return MULIB_SC_INVALID;
  int r = !mulib_scr_valid_(p) || !p->random ? MULIB_SC_UNSUPPORTED
    : p->random(p->ctx, (unsigned char *)out, n);
  return mulib_scr_finish_(r, out, n);
}

int mulib_scr_hash(const mulib_scr_provider *p, unsigned a, mulib_sc_view in,
                   unsigned char *out, size_t cap)
{
  size_t n = mulib_scr_digest_size(a);
  if (!n || cap < n || !mulib_scr_output_(p, out, n)
      || !mulib_sc_view_ok(in) || mulib_scr_alias_(out, n, in)) return MULIB_SC_INVALID;
  int r = !mulib_scr_valid_(p) || !p->hash ? MULIB_SC_UNSUPPORTED
    : p->hash(p->ctx, a, in, out, n);
  return mulib_scr_finish_(r, out, n);
}

int mulib_scr_hmac(const mulib_scr_provider *p, unsigned a, mulib_sc_view key,
                   mulib_sc_view in, unsigned char *out, size_t cap)
{
  size_t n = mulib_scr_digest_size(a);
  if (!n || cap < n || !mulib_scr_output_(p, out, n)
      || !mulib_sc_view_ok(in) || !mulib_sc_view_ok(key)
      || mulib_scr_alias_(out, n, in) || mulib_scr_alias_(out, n, key))
    return MULIB_SC_INVALID;
  int r = !mulib_scr_valid_(p) || !p->hmac ? MULIB_SC_UNSUPPORTED
    : p->hmac(p->ctx, a, key, in, out, n);
  return mulib_scr_finish_(r, out, n);
}

int mulib_scr_hkdf(const mulib_scr_provider *p, unsigned a, mulib_sc_view salt,
                   mulib_sc_view ikm, mulib_sc_view info, unsigned char *out, size_t n)
{
  size_t d = mulib_scr_digest_size(a);
  unsigned char prk[64] = {0}, t[64] = {0}, buf[64 + 1024 + 1];
  size_t pos = 0, prev = 0;
  if (a == MULIB_SCR_SHA1 || !d || n > 255 * d || info.len > 1024
      || !mulib_scr_output_(p, out, n) || !mulib_sc_view_ok(salt)
      || !mulib_sc_view_ok(ikm) || !mulib_sc_view_ok(info) || mulib_scr_alias_(out, n, salt)
      || mulib_scr_alias_(out, n, ikm) || mulib_scr_alias_(out, n, info))
    return MULIB_SC_INVALID;
  int r = mulib_scr_hmac(p, a, salt, ikm, prk, sizeof prk);
  for (unsigned i = 1; !r && pos < n; ++i) {
    if (prev) memcpy(buf, t, prev);
    if (info.len) memcpy(buf + prev, info.ptr, info.len);
    buf[prev + info.len] = (unsigned char)i;
    mulib_sc_view k = {prk, d}, m = {buf, prev + info.len + 1};
    r = mulib_scr_hmac(p, a, k, m, t, sizeof t);
    if (!r) {
      size_t take = n - pos < d ? n - pos : d;
      memcpy(out + pos, t, take);
      pos += take;
      prev = d;
    }
  }
  mulib_sc_wipe(prk, sizeof prk);
  mulib_sc_wipe(t, sizeof t);
  mulib_sc_wipe(buf, sizeof buf);
  return mulib_scr_finish_(r, out, n);
}

int mulib_scr_kdf(const mulib_scr_provider *p, mulib_scr_kdf_params q,
                  mulib_sc_view pass, mulib_sc_view salt, unsigned char out[32])
{
  if (!mulib_scr_output_(p, out, 32) || !mulib_sc_view_ok(pass)
      || !pass.len || pass.len > 4096 || !mulib_sc_view_ok(salt) || salt.len < 16
      || salt.len > 64 || mulib_scr_alias_(out, 32, pass) || mulib_scr_alias_(out, 32, salt))
    return MULIB_SC_INVALID;
  if (q.algorithm == MULIB_SCR_PBKDF2_SHA256) {
    if (q.iterations < 600000 || q.iterations > 10000000 || q.memory_kib || q.lanes)
      return MULIB_SC_LIMIT;
  } else if (q.algorithm == MULIB_SCR_ARGON2ID) {
    if (q.iterations < 2 || q.iterations > 10 || q.memory_kib < 19456
        || q.memory_kib > 1048576 || !q.lanes || q.lanes > 16)
      return MULIB_SC_LIMIT;
  } else {
    return MULIB_SC_UNSUPPORTED;
  }
  int r = !mulib_scr_valid_(p) || !p->kdf ? MULIB_SC_UNSUPPORTED
    : p->kdf(p->ctx, q, pass, salt, out);
  return mulib_scr_finish_(r, out, 32);
}

static int mulib_scr_aead_args_(const mulib_scr_provider *p, unsigned a,
                                mulib_sc_view key, mulib_sc_view nonce, mulib_sc_view aad,
                                mulib_sc_view in, mulib_sc_buf out, const unsigned char *tag)
{
  if (!mulib_scr_key_size(a) || !key.ptr || key.len != mulib_scr_key_size(a)
      || !nonce.ptr || nonce.len != mulib_scr_nonce_size(a) || !tag
      || !mulib_sc_view_ok(aad) || !mulib_sc_view_ok(in)
      || (!out.ptr && out.cap) || out.cap < in.len)
    return MULIB_SC_INVALID;
  if (in.len > UINT32_C(64) * 1024 * 1024 || aad.len > UINT32_C(64) * 1024 * 1024)
    return MULIB_SC_LIMIT;
  if (!mulib_scr_output_(p, out.ptr, in.len)
      || mulib_scr_alias_(out.ptr, in.len, key) || mulib_scr_alias_(out.ptr, in.len, nonce)
      || mulib_scr_alias_(out.ptr, in.len, aad) || mulib_scr_alias_(out.ptr, in.len, in)
      || mulib_sc_overlap(out.ptr, in.len, tag, 16))
    return MULIB_SC_INVALID;
  return MULIB_SC_OK;
}

int mulib_scr_seal(const mulib_scr_provider *p, unsigned a, mulib_sc_view key,
                   mulib_sc_view nonce, mulib_sc_view aad, mulib_sc_view in,
                   mulib_sc_buf out, unsigned char tag[16])
{
  int r = mulib_scr_aead_args_(p, a, key, nonce, aad, in, out, tag);
  if (r) return r;
  if (!mulib_scr_output_(p, tag, 16) || mulib_scr_alias_(tag, 16, key)
      || mulib_scr_alias_(tag, 16, nonce) || mulib_scr_alias_(tag, 16, aad)
      || mulib_scr_alias_(tag, 16, in)) return MULIB_SC_INVALID;
  r = !mulib_scr_valid_(p) || !p->seal ? MULIB_SC_UNSUPPORTED
    : p->seal(p->ctx, a, key, nonce, aad, in, out.ptr, tag);
  if (r) mulib_sc_wipe(tag, 16);
  return mulib_scr_finish_(r, out.ptr, in.len);
}

int mulib_scr_open(const mulib_scr_provider *p, unsigned a, mulib_sc_view key,
                   mulib_sc_view nonce, mulib_sc_view aad, mulib_sc_view in,
                   const unsigned char tag[16], mulib_sc_buf out)
{
  int r = mulib_scr_aead_args_(p, a, key, nonce, aad, in, out, tag);
  if (r) return r;
  r = !mulib_scr_valid_(p) || !p->open ? MULIB_SC_UNSUPPORTED
    : p->open(p->ctx, a, key, nonce, aad, in, tag, out.ptr);
  return mulib_scr_finish_(r, out.ptr, in.len);
}
#ifdef __cplusplus
}
#endif
#endif
