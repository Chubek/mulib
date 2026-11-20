/** @file mulib-secret-random.h
 * @brief Unbiased random selections, identifiers, recovery codes, password policies.
 * Define MULIB_IMPL_SECRET_RANDOM once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_RANDOM_H
#define MULIB_SECRET_RANDOM_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_SRAND_LOWER = 1, MULIB_SRAND_UPPER = 2,
  MULIB_SRAND_DIGIT = 4, MULIB_SRAND_SYMBOL = 8
};
/* Use a cryptographic provider. Invalid arguments leave outputs unchanged.
 * A failed draw leaves the scalar unchanged; generation failures clear the
 * intended text/UUID output. Alphabet and output must not overlap. */
int mulib_srand_uniform(const mulib_scr_provider *p, uint32_t bound, uint32_t *out);
/* Unique ASCII graphic alphabet (2–94 characters); NUL-terminated result.
 * This primitive does not require any particular entropy or composition. */
int mulib_srand_text(const mulib_scr_provider *p, const char *alphabet,
                     size_t count, char *out, size_t cap);
/* Uniform over strings containing every selected class (no biased repair).
 * Callers must set an appropriate length/entropy policy. */
int mulib_srand_password(const mulib_scr_provider *p, unsigned classes,
                         size_t n, char *out, size_t cap);
int mulib_srand_uuid(const mulib_scr_provider *p,unsigned char id[16]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_RANDOM) && !defined(MULIB_SECRET_RANDOM_IMPLEMENTED)
#define MULIB_SECRET_RANDOM_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int
mulib_srand_uniform(const mulib_scr_provider *p, uint32_t bound, uint32_t *out)
{
  if (!bound || !out) return MULIB_SC_INVALID;
  uint32_t threshold = (uint32_t)(0u - bound) % bound;
  unsigned char bytes[4];
  uint32_t value = 0;
  int rc = MULIB_SC_IO;
  for (unsigned i = 0; i < 128; ++i) {
    rc = mulib_scr_random(p, bytes, sizeof bytes);
    if (rc) break;
    value = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16)
            | ((uint32_t)bytes[2] << 8) | bytes[3];
    if (value >= threshold) {
      *out = value % bound;
      break;
    }
    rc = MULIB_SC_IO;
  }
  mulib_sc_wipe(bytes, sizeof bytes);
  mulib_sc_wipe(&value, sizeof value);
  return rc;
}

/* Accept a whole multiple of count in [0, 256), so each index has equal
 * probability. The work bound aborts instead of repairing a rejected draw. */
static int
mulib_srand_sample_(const mulib_scr_provider *p, const char *alphabet,
                    unsigned count, const unsigned char *classes,
                    size_t n, char *out, unsigned *present)
{
  unsigned cutoff = 256u - 256u % count;
  unsigned char bytes[64];
  size_t pos = 0, budget = 4 * n + sizeof bytes;
  unsigned got = 0;
  int rc = MULIB_SC_IO;
  while (pos < n && budget) {
    size_t fetch = n - pos;
    if (fetch < 16) fetch = 16;
    if (fetch > sizeof bytes) fetch = sizeof bytes;
    if (fetch > budget) fetch = budget;
    rc = mulib_scr_random(p, bytes, fetch);
    if (rc) break;
    budget -= fetch;
    for (size_t i = 0; i < fetch && pos < n; ++i) {
      if (bytes[i] >= cutoff) continue;
      unsigned index = bytes[i] % count;
      out[pos++] = alphabet[index];
      if (classes) got |= classes[index];
    }
    if (pos < n) rc = MULIB_SC_IO;
  }
  mulib_sc_wipe(bytes, sizeof bytes);
  if (rc) mulib_sc_wipe(out, n + 1);
  else {
    out[n] = 0;
    if (present) *present = got;
  }
  return rc;
}

int
mulib_srand_text(const mulib_scr_provider *p, const char *alphabet,
                size_t n, char *out, size_t cap)
{
  if (!alphabet || !out || !n || n > 4096 || cap <= n) return MULIB_SC_INVALID;
  unsigned char seen[128] = {0};
  unsigned count = 0;
  for (; alphabet[count]; ++count) {
    unsigned c = (unsigned char)alphabet[count];
    if (count == 94 || c < 33 || c > 126 || seen[c])
      return MULIB_SC_INVALID;
    seen[c] = 1;
  }
  if (count < 2 || mulib_sc_overlap(alphabet, count + 1, out, n + 1))
    return MULIB_SC_INVALID;
  return mulib_srand_sample_(p, alphabet, count, NULL, n, out, NULL);
}

int
mulib_srand_password(const mulib_scr_provider *p, unsigned classes,
                    size_t n, char *out, size_t cap)
{
  static const char *const sets[] = {
    "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "0123456789", "!@#$%^&*()-_=+[]"
  };
  char alphabet[94];
  unsigned char mask[94];
  unsigned count = 0, required = 0;
  if (!classes || (classes & ~15u) || !out || cap <= n || n > 4096)
    return MULIB_SC_INVALID;
  for (unsigned i = 0; i < 4; ++i) {
    if (!(classes & (1u << i))) continue;
    size_t len = strlen(sets[i]);
    memcpy(alphabet + count, sets[i], len);
    memset(mask + count, (int)(1u << i), len);
    count += (unsigned)len;
    ++required;
  }
  if (n < required) return MULIB_SC_INVALID;
  for (unsigned attempt = 0; attempt < 1024; ++attempt) {
    unsigned present = 0;
    int rc = mulib_srand_sample_(p, alphabet, count, mask, n, out, &present);
    if (rc) return rc;
    if (present == classes) return MULIB_SC_OK;
  }
  mulib_sc_wipe(out, n + 1);
  return MULIB_SC_LIMIT;
}

int
mulib_srand_uuid(const mulib_scr_provider *p, unsigned char id[16])
{
  if (!id) return MULIB_SC_INVALID;
  int rc = mulib_scr_random(p, id, 16);
  if (!rc) {
    id[6] = (unsigned char)((id[6] & 15) | 64);
    id[8] = (unsigned char)((id[8] & 63) | 128);
  }
  return rc;
}
#ifdef __cplusplus
}
#endif
#endif
