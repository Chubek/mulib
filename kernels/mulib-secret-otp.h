/** @file mulib-secret-otp.h
 * @brief HOTP/TOTP SHA1/256/512, Steam format, replay-safe persistent HOTP issuance.
 * Define MULIB_IMPL_SECRET_OTP once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_OTP_H
#define MULIB_SECRET_OTP_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#include "mulib-secret-state.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Code outputs are NUL terminated and left unchanged on failure. Inputs,
 * outputs, and state must not overlap. `remaining` is optional. */
int mulib_sotp_hotp(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                    uint64_t counter, unsigned digits, char *out, size_t cap);
int mulib_sotp_totp(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                    uint64_t now, uint64_t epoch, uint32_t period, unsigned digits,
                    char *out, size_t cap, uint32_t *remaining);
int mulib_sotp_steam(const mulib_scr_provider *p, mulib_sc_view key, uint64_t now,
                     char out[6]);
/* Release the code only after the counter has durably advanced. */
int mulib_sotp_next(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                    unsigned digits, mulib_ss_counter *state, mulib_ss_cas cas,
                    void *ctx, char *out, size_t cap);
/* Verify decimal bytes (not a C string); allow at most 10 steps on each side.
 * Require a single match in the entire window, then durably consume the step
 * in a dedicated counter (initial counter 0, nonzero revision). Return AUTH
 * for a mismatch, ambiguous match or replay. See mulib_ss_accept for reload
 * after a conflict or uncertain commit. Host supplies trusted time, pins the
 * key/epoch/period/skew and enforces rate limits and state rollback policy. */
int mulib_sotp_totp_verify(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                           uint64_t now, uint64_t epoch, uint32_t period, unsigned digits,
                           mulib_sc_view code, unsigned past, unsigned future,
                           mulib_ss_counter *state, mulib_ss_cas cas, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_OTP) && !defined(MULIB_SECRET_OTP_IMPLEMENTED)
#define MULIB_SECRET_OTP_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int
mulib_sotp_value_(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                  uint64_t counter, uint32_t *value)
{
  if ((hash != MULIB_SCR_SHA1 && hash != MULIB_SCR_SHA256 && hash != MULIB_SCR_SHA512)
      || !key.ptr || key.len < 10 || key.len > 4096) return MULIB_SC_INVALID;
  unsigned char message[8], digest[64];
  mulib_sc_put64(message, counter);
  mulib_sc_view in = {message, sizeof message};
  int rc = mulib_scr_hmac(p, hash, key, in, digest, sizeof digest);
  if (!rc) {
    unsigned i = digest[mulib_scr_digest_size(hash) - 1] & 15;
    *value = ((uint32_t)(digest[i] & 127) << 24) | ((uint32_t)digest[i + 1] << 16)
              | ((uint32_t)digest[i + 2] << 8) | digest[i + 3];
  }
  mulib_sc_wipe(digest, sizeof digest);
  return rc;
}

int
mulib_sotp_hotp(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                uint64_t counter, unsigned digits, char *out, size_t cap)
{
  if (!out || digits < 6 || digits > 8 || cap <= digits
      || mulib_sc_overlap(out, digits + 1, key.ptr, key.len))
    return MULIB_SC_INVALID;
  uint32_t value;
  int rc = mulib_sotp_value_(p, hash, key, counter, &value);
  if (!rc) {
    out[digits] = 0;
    for (unsigned i = digits; i; --i) {
      out[i - 1] = (char)('0' + value % 10);
      value /= 10;
    }
  }
  mulib_sc_wipe(&value, sizeof value);
  return rc;
}

int
mulib_sotp_totp(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                uint64_t now, uint64_t epoch, uint32_t period, unsigned digits,
                char *out, size_t cap, uint32_t *remaining)
{
  if (!period || now < epoch || digits < 6 || digits > 8
      || mulib_sc_overlap(out, digits + 1, remaining, sizeof *remaining)
      || mulib_sc_overlap(key.ptr, key.len, remaining, sizeof *remaining))
    return MULIB_SC_INVALID;
  uint64_t elapsed = now - epoch;
  int rc = mulib_sotp_hotp(p, hash, key, elapsed / period, digits, out, cap);
  if (!rc && remaining) *remaining = period - (uint32_t)(elapsed % period);
  return rc;
}

int
mulib_sotp_steam(const mulib_scr_provider *p, mulib_sc_view key, uint64_t now,
                 char out[6])
{
  if (!out || mulib_sc_overlap(out, 6, key.ptr, key.len)) return MULIB_SC_INVALID;
  static const char alphabet[] = "23456789BCDFGHJKMNPQRTVWXY";
  uint32_t value;
  int rc = mulib_sotp_value_(p, MULIB_SCR_SHA1, key, now / 30, &value);
  if (!rc) {
    unsigned base = sizeof alphabet - 1;
    for (unsigned i = 0; i < 5; ++i) {
      out[i] = alphabet[value % base];
      value /= base;
    }
    out[5] = 0;
  }
  mulib_sc_wipe(&value, sizeof value);
  return rc;
}

int
mulib_sotp_next(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                unsigned digits, mulib_ss_counter *state, mulib_ss_cas cas,
                void *ctx, char *out, size_t cap)
{
  if (!state || !cas || !out || digits < 6 || digits > 8 || cap <= digits
      || mulib_sc_overlap(out, digits + 1, state, sizeof *state)
      || mulib_sc_overlap(out, digits + 1, key.ptr, key.len)
      || mulib_sc_overlap(state, sizeof *state, key.ptr, key.len))
    return MULIB_SC_INVALID;
  if (!state->revision) return MULIB_SC_STATE;
  if (state->counter == UINT64_MAX || state->revision == UINT64_MAX)
    return MULIB_SC_LIMIT;
  char code[9];
  uint64_t counter = state->counter, reserved;
  int rc = mulib_sotp_hotp(p, hash, key, counter, digits, code, sizeof code);
  if (!rc) rc = mulib_ss_reserve(state, cas, ctx, &reserved);
  if (!rc) memcpy(out, code, digits + 1);
  mulib_sc_wipe(code, sizeof code);
  return rc;
}

int
mulib_sotp_totp_verify(const mulib_scr_provider *p, unsigned hash, mulib_sc_view key,
                       uint64_t now, uint64_t epoch, uint32_t period, unsigned digits,
                       mulib_sc_view code, unsigned past, unsigned future,
                       mulib_ss_counter *state, mulib_ss_cas cas, void *ctx)
{
  if (!state || !cas || !period || now < epoch || digits < 6 || digits > 8
      || !code.ptr || code.len != digits || past > 10 || future > 10
      || mulib_sc_overlap(state, sizeof *state, code.ptr, code.len)
      || mulib_sc_overlap(state, sizeof *state, key.ptr, key.len))
    return MULIB_SC_INVALID;
  if (!state->revision) return MULIB_SC_STATE;
  for (size_t i = 0; i < code.len; ++i)
    if (code.ptr[i] < '0' || code.ptr[i] > '9') return MULIB_SC_INVALID;

  uint64_t step = (now - epoch) / period;
  uint64_t first = step < past ? 0 : step - past;
  uint64_t last = UINT64_MAX - step < future ? UINT64_MAX : step + future;
  uint64_t match = 0;
  unsigned matches = 0;
  char expected[9];
  int rc;
  for (uint64_t i = first;; ++i) {
    rc = mulib_sotp_hotp(p, hash, key, i, digits, expected, sizeof expected);
    if (rc) break;
    if (mulib_sc_equal(expected, code.ptr, digits)) {
      match = i;
      ++matches;
    }
    if (i == last) break;
  }
  mulib_sc_wipe(expected, sizeof expected);
  if (rc) return rc;
  if (matches != 1) return MULIB_SC_AUTH;
  return mulib_ss_accept(state, match, cas, ctx);
}
#ifdef __cplusplus
}
#endif
#endif
