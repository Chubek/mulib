/** @file mulib-secret-recovery.h
 * @brief Shamir GF(256) threshold secret sharing with strict share identifiers.
 * Define MULIB_IMPL_SECRET_RECOVERY once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_RECOVERY_H
#define MULIB_SECRET_RECOVERY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Raw shares (one nonzero x-byte followed by n data bytes) are NOT authenticated.
 * Use encrypted/authenticated envelopes and bind vault, set ID, threshold and
 * version. Reconstruction cannot detect a malicious or wrong share.
 * Join takes exactly threshold shares. Inputs/output must not overlap.
 * Invalid arguments leave output unchanged; split clears partial output
 * if the random provider fails. */
int mulib_srec_split(const mulib_scr_provider *p,mulib_sc_view secret,unsigned threshold,unsigned count,mulib_sc_buf shares);
int mulib_srec_join(mulib_sc_view shares,size_t secret_length,unsigned threshold,unsigned count,mulib_sc_buf out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_RECOVERY) && !defined(MULIB_SECRET_RECOVERY_IMPLEMENTED)
#define MULIB_SECRET_RECOVERY_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static unsigned char
mulib_srec_mul_(unsigned char a, unsigned char b)
{
  unsigned char r = 0;
  for (unsigned i = 0; i < 8; ++i) {
    r ^= (unsigned char)(a & (0u - (b & 1u)));
    unsigned high = a >> 7;
    a = (unsigned char)((a << 1) ^ (0x1bu & (0u - high)));
    b >>= 1;
  }
  return r;
}

static unsigned char
mulib_srec_inv_(unsigned char a)
{
  unsigned char x = 1;
  for (unsigned i = 0; i < 8; ++i) {
    x = mulib_srec_mul_(x, x);
    if ((254u >> (7 - i)) & 1u) x = mulib_srec_mul_(x, a);
  }
  return x;
}

int
mulib_srec_split(const mulib_scr_provider *p, mulib_sc_view secret, unsigned threshold,
                  unsigned count, mulib_sc_buf shares)
{
  if (!secret.ptr || !secret.len || secret.len > 4096 || threshold < 2
      || count < threshold || count > 255 || !shares.ptr) return MULIB_SC_INVALID;
  size_t stride = secret.len + 1;
  if (stride > SIZE_MAX / count || shares.cap < stride * count) return MULIB_SC_LIMIT;
  if (mulib_sc_overlap(secret.ptr, secret.len, shares.ptr, stride * count))
    return MULIB_SC_INVALID;
  unsigned char coeff[255];
  int rc = MULIB_SC_OK;
  for (unsigned i = 0; i < count; ++i) shares.ptr[i * stride] = (unsigned char)(i + 1);
  for (size_t j = 0; j < secret.len; ++j) {
    coeff[0] = secret.ptr[j];
    rc = mulib_scr_random(p, coeff + 1, threshold - 1);
    if (rc) break;
    for (unsigned i = 0; i < count; ++i) {
      unsigned char x = (unsigned char)(i + 1), y = coeff[threshold - 1];
      for (unsigned k = threshold - 1; k; --k)
        y = mulib_srec_mul_(y, x) ^ coeff[k - 1];
      shares.ptr[i * stride + j + 1] = y;
    }
  }
  if (rc) mulib_sc_wipe(shares.ptr, stride * count);
  mulib_sc_wipe(coeff, sizeof coeff);
  return rc;
}

int
mulib_srec_join(mulib_sc_view shares, size_t len, unsigned threshold,
                 unsigned count, mulib_sc_buf out)
{
  if (!shares.ptr || !len || len > 4096 || threshold < 2 || count != threshold
      || count > 255 || !out.ptr) return MULIB_SC_INVALID;
  size_t stride = len + 1;
  if (stride > SIZE_MAX / count || shares.len != stride * count
      || mulib_sc_overlap(shares.ptr, shares.len, out.ptr, len)) return MULIB_SC_INVALID;
  if (out.cap < len) return MULIB_SC_LIMIT;
  unsigned char seen[256] = {0}, weights[255];
  for (unsigned i = 0; i < count; ++i) {
    unsigned char x = shares.ptr[i * stride];
    if (!x || seen[x]) return MULIB_SC_INVALID;
    seen[x] = 1;
  }
  for (unsigned i = 0; i < count; ++i) {
    unsigned char xi = shares.ptr[i * stride], num = 1, den = 1;
    for (unsigned j = 0; j < count; ++j) {
      if (i == j) continue;
      unsigned char xj = shares.ptr[j * stride];
      num = mulib_srec_mul_(num, xj);
      den = mulib_srec_mul_(den, xi ^ xj);
    }
    weights[i] = mulib_srec_mul_(num, mulib_srec_inv_(den));
  }
  for (size_t j = 0; j < len; ++j) {
    unsigned char value = 0;
    for (unsigned i = 0; i < count; ++i)
      value ^= mulib_srec_mul_(weights[i], shares.ptr[i * stride + j + 1]);
    out.ptr[j] = value;
  }
  return MULIB_SC_OK;
}
#ifdef __cplusplus
}
#endif
#endif
