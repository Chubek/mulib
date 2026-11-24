/** @file mulib-secret-passphrase.h
 * @brief Word-list passphrases and high-entropy recovery-code verifiers.
 * Define MULIB_IMPL_SECRET_PASSPHRASE once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_PASSPHRASE_H
#define MULIB_SECRET_PASSPHRASE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-random.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Caller supplies a vetted list of 1024–65536 words. All entries must be
 * distinct, 1–64 ASCII graphic characters, and exclude the separator.
 * Require capacity for the longest possible result, including its NUL.
 * The borrowed list/words must stay immutable and disjoint from the output.
 * Validation/capacity failures leave output unchanged; draw failures clear
 * bytes already written plus a terminator. Select enough words for the
 * application policy. The separator must be an ASCII graphic character. */
int mulib_sphrase_generate(const mulib_scr_provider *p, const char *const *words,
                            size_t count, unsigned word_count, char separator,
                            char *out, size_t cap);
/* Recovery codes contain 20 independently sampled base32 symbols (~100 bits).
 * Store the HMAC under a dedicated recovery key, compare it in constant time,
 * and consume codes once in a durable transaction. These helpers do not store
 * or consume codes. Verifier output is untouched for invalid arguments and
 * cleared if the HMAC operation fails. */
int mulib_sphrase_recovery(const mulib_scr_provider *p,char code[21]);
int mulib_sphrase_verifier(const mulib_scr_provider *p,mulib_sc_view key,const char *code,unsigned char out[32]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_PASSPHRASE) && !defined(MULIB_SECRET_PASSPHRASE_IMPLEMENTED)
#define MULIB_SECRET_PASSPHRASE_IMPLEMENTED
#include <string.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
static int
mulib_sphrase_compare_(const void *a, const void *b)
{
  return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int
mulib_sphrase_generate(const mulib_scr_provider *p, const char *const *words,
                       size_t n, unsigned count, char sep, char *out, size_t cap)
{
  if (!words || n < 1024 || n > 65536 || count < 4 || count > 32
      || !out || sep < 33 || sep > 126) return MULIB_SC_INVALID;

  size_t longest = 0;
  for (size_t i = 0; i < n; ++i) {
    const char *word = words[i];
    if (!word || !word[0]) return MULIB_SC_INVALID;
    size_t len = 0;
    for (; word[len]; ++len)
      if (len == 64 || word[len] < 33 || word[len] > 126 || word[len] == sep)
        return MULIB_SC_INVALID;
    if (len > longest) longest = len;
  }
  if (cap < (longest + 1) * count || n > SIZE_MAX / sizeof *words)
    return MULIB_SC_LIMIT;
  size_t need = (longest + 1) * count;
  if (mulib_sc_overlap(words, n * sizeof *words, out, need)) return MULIB_SC_INVALID;
  for (size_t i = 0; i < n; ++i)
    if (mulib_sc_overlap(words[i], strlen(words[i]) + 1, out, need))
      return MULIB_SC_INVALID;
  const char **sorted = (const char **)malloc(n * sizeof *sorted);
  if (!sorted) return MULIB_SC_NOMEM;
  memcpy(sorted, words, n * sizeof *sorted);
  qsort(sorted, n, sizeof *sorted, mulib_sphrase_compare_);
  int rc = MULIB_SC_OK;
  for (size_t i = 1; i < n; ++i)
    if (!strcmp(sorted[i - 1], sorted[i])) {
      rc = MULIB_SC_INVALID;
      break;
    }
  free(sorted);
  if (rc) return rc;

  size_t pos = 0;
  uint32_t index = 0;
  for (unsigned i = 0; i < count; ++i) {
    rc = mulib_srand_uniform(p, (uint32_t)n, &index);
    if (rc) break;
    const char *word = words[index];
    size_t len = strlen(word);
    if (i) out[pos++] = sep;
    memcpy(out + pos, word, len);
    pos += len;
  }
  mulib_sc_wipe(&index, sizeof index);
  if (rc) mulib_sc_wipe(out, pos + 1);
  else out[pos] = 0;
  return rc;
}

int
mulib_sphrase_recovery(const mulib_scr_provider *p, char out[21])
{
  return mulib_srand_text(p, "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", 20, out, 21);
}

int
mulib_sphrase_verifier(const mulib_scr_provider *p, mulib_sc_view key,
                       const char *code, unsigned char out[32])
{
  static const char domain[] = "MuLIB.recovery.v1";
  if (!code || !out || !key.ptr || key.len < 32) return MULIB_SC_INVALID;
  for (unsigned i = 0; i < 20; ++i)
    if (!code[i] || !strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", code[i]))
      return MULIB_SC_INVALID;
  if (code[20] || mulib_sc_overlap(out, 32, code, 21)
      || mulib_sc_overlap(out, 32, key.ptr, key.len)) return MULIB_SC_INVALID;
  unsigned char message[sizeof domain - 1 + 20];
  memcpy(message, domain, sizeof domain - 1);
  memcpy(message + sizeof domain - 1, code, 20);
  mulib_sc_view view = {message, sizeof message};
  int rc = mulib_scr_hmac(p, MULIB_SCR_SHA256, key, view, out, 32);
  if (rc) mulib_sc_wipe(out, 32);
  mulib_sc_wipe(message, sizeof message);
  return rc;
}
#ifdef __cplusplus
}
#endif
#endif
