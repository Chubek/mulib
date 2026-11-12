/** @file mulib-webextract-evidence.h
 * @brief Source-span anchored extraction evidence records and integrity
 * hashes. C11 single-header module. Define MULIB_IMPL_WEBEXTRACT_EVIDENCE in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_WEBEXTRACT_EVIDENCE_H
#define MULIB_WEBEXTRACT_EVIDENCE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_webextract_evidence
  {
    size_t start, length;
    uint64_t document_hash, span_hash;
    char field[64];
  } mulib_webextract_evidence_t;
  /** @brief See manual/mulib-webextract-evidence.md for this operation's
   * contract. */
  int mulib_webextract_evidence_make (const char *document,
                                      size_t document_length, size_t offset,
                                      size_t length, const char *field,
                                      mulib_webextract_evidence_t *out);
  /** @brief See manual/mulib-webextract-evidence.md for this operation's
   * contract. */
  int mulib_webextract_evidence_verify (
      const char *document, size_t document_length,
      const mulib_webextract_evidence_t *evidence);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBEXTRACT_EVIDENCE_H */
#ifdef MULIB_IMPL_WEBEXTRACT_EVIDENCE
#ifndef MULIB_IMPL_WEBEXTRACT_EVIDENCE_ONCE
#define MULIB_IMPL_WEBEXTRACT_EVIDENCE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_webextract_evidence_make (const char *s, size_t n, size_t off,
                                size_t len, const char *field,
                                mulib_webextract_evidence_t *o)
{
  if (!s || !field || !*field || !o || off > n || len > n - off)
    return 0;
  mulib_webextract_evidence_t z = { off,
                                    len,
                                    mulib_shylock_hash (s, n),
                                    mulib_shylock_hash (s + off, len),
                                    { 0 } };
  if (!mulib_shylock_copy (z.field, sizeof z.field, field))
    return 0;
  *o = z;
  return 1;
}
int
mulib_webextract_evidence_verify (const char *s, size_t n,
                                  const mulib_webextract_evidence_t *e)
{
  return s && e && e->start <= n && e->length <= n - e->start
         && e->document_hash == mulib_shylock_hash (s, n)
         && e->span_hash == mulib_shylock_hash (s + e->start, e->length);
}
#endif
#endif
