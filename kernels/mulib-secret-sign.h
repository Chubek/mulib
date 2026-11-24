/** @file mulib-secret-sign.h
 * @brief Opaque signing/key-agreement backend contract and algorithm-pinned dispatch.
 * Define MULIB_IMPL_SECRET_SIGN once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_SIGN_H
#define MULIB_SECRET_SIGN_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-core.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_SSIGN_ED25519=1,MULIB_SSIGN_ECDSA_P256=2,MULIB_SSIGN_RSA_PSS_SHA256=3,MULIB_SSIGN_X25519=4,MULIB_SSIGN_ML_DSA65=5 };
typedef struct { unsigned char id[32]; unsigned algorithm; unsigned usage; } mulib_ssign_key;
enum { MULIB_SSIGN_SIGN=1,MULIB_SSIGN_VERIFY=2,MULIB_SSIGN_AGREE=4 };
typedef struct { uint32_t abi; void *ctx; int (*sign)(void*,const mulib_ssign_key*,mulib_sc_view,mulib_sc_buf,size_t*); int (*verify)(void*,const mulib_ssign_key*,mulib_sc_view,mulib_sc_view); int (*agree)(void*,const mulib_ssign_key*,mulib_sc_view,unsigned char[32]); } mulib_ssign_backend;
/* This only checks signature shape: Ed25519 and raw P-256 r||s use 64 bytes;
 * RSA-PSS is 256–1024 bytes (backend pins exact key size and PSS parameters).
 * Backends must enforce all cryptographic and key-specific constraints.
 * Output and written must be disjoint from inputs and each other. Failed
 * dispatches clear outputs, while precondition errors leave them unchanged. */
int mulib_ssign_sign(const mulib_ssign_backend *b,const mulib_ssign_key *k,mulib_sc_view msg,mulib_sc_buf sig,size_t *written);
int mulib_ssign_verify(const mulib_ssign_backend *b,const mulib_ssign_key *k,mulib_sc_view msg,mulib_sc_view sig);
int mulib_ssign_agree(const mulib_ssign_backend *b,const mulib_ssign_key *k,mulib_sc_view peer,unsigned char shared[32]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_SIGN) && !defined(MULIB_SECRET_SIGN_IMPLEMENTED)
#define MULIB_SECRET_SIGN_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static size_t
mulib_ssign_min_(unsigned algorithm)
{
  switch (algorithm) {
    case MULIB_SSIGN_ED25519: case MULIB_SSIGN_ECDSA_P256: return 64;
    case MULIB_SSIGN_RSA_PSS_SHA256: return 256;
    case MULIB_SSIGN_ML_DSA65: return 3309;
    default: return 0;
  }
}

static int
mulib_ssign_len_(unsigned algorithm, size_t n)
{
  size_t min = mulib_ssign_min_(algorithm);
  return min && (algorithm == MULIB_SSIGN_RSA_PSS_SHA256 ? n >= min && n <= 1024 : n == min);
}

int
mulib_ssign_sign(const mulib_ssign_backend *b, const mulib_ssign_key *key,
                  mulib_sc_view msg, mulib_sc_buf out, size_t *written)
{
  if (!key || !(key->usage & MULIB_SSIGN_SIGN) || !mulib_sc_view_ok(msg)
      || !out.ptr || !written || out.cap > 16384
      || mulib_sc_overlap(out.ptr, out.cap, key, sizeof *key)
      || mulib_sc_overlap(out.ptr, out.cap, msg.ptr, msg.len)
      || mulib_sc_overlap(out.ptr, out.cap, written, sizeof *written)
      || mulib_sc_overlap(written, sizeof *written, key, sizeof *key)
      || mulib_sc_overlap(written, sizeof *written, msg.ptr, msg.len))
    return MULIB_SC_INVALID;
  size_t min = mulib_ssign_min_(key->algorithm);
  if (!min) return MULIB_SC_INVALID;
  if (out.cap < min) return MULIB_SC_LIMIT;
  if (!b || b->abi != 1 || !b->sign) return MULIB_SC_UNSUPPORTED;
  size_t n = 0;
  int rc = b->sign(b->ctx, key, msg, out, &n);
  if (!rc && (n > out.cap || !mulib_ssign_len_(key->algorithm, n))) rc = MULIB_SC_INVALID;
  if (rc) mulib_sc_wipe(out.ptr, out.cap);
  else *written = n;
  return rc;
}

int
mulib_ssign_verify(const mulib_ssign_backend *b, const mulib_ssign_key *key,
                    mulib_sc_view msg, mulib_sc_view sig)
{
  if (!key || !(key->usage & MULIB_SSIGN_VERIFY) || !mulib_sc_view_ok(msg)
      || !sig.ptr || !mulib_ssign_len_(key->algorithm, sig.len))
    return MULIB_SC_INVALID;
  if (!b || b->abi != 1 || !b->verify) return MULIB_SC_UNSUPPORTED;
  return b->verify(b->ctx, key, msg, sig);
}

int
mulib_ssign_agree(const mulib_ssign_backend *b, const mulib_ssign_key *key,
                   mulib_sc_view peer, unsigned char out[32])
{
  if (!key || !(key->usage & MULIB_SSIGN_AGREE) || key->algorithm != MULIB_SSIGN_X25519
      || !peer.ptr || peer.len != 32 || !out
      || mulib_sc_overlap(out, 32, key, sizeof *key)
      || mulib_sc_overlap(out, 32, peer.ptr, peer.len)) return MULIB_SC_INVALID;
  if (!b || b->abi != 1 || !b->agree) return MULIB_SC_UNSUPPORTED;
  unsigned char zero[32] = {0}, shared[32] = {0};
  int rc = mulib_sc_equal(peer.ptr, zero, sizeof zero)
         ? MULIB_SC_AUTH : b->agree(b->ctx, key, peer, shared);
  if (!rc && mulib_sc_equal(shared, zero, sizeof zero)) rc = MULIB_SC_AUTH;
  if (rc) mulib_sc_wipe(out, 32);
  else memcpy(out, shared, sizeof shared);
  mulib_sc_wipe(shared, sizeof shared);
  return rc;
}
#ifdef __cplusplus
}
#endif
#endif
