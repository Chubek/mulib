/** @file mulib-unit.h
 * @brief Shared owned architecture profiles for MPU, MCU and GPU contracts.
 * Define MULIB_IMPL_UNIT, ARCHML, PARSEC and EMITTER explicitly once.
 * Zero initialize contexts; never copy ownership or modify their fields.
 * Builtins are generated from library/archml; no runtime files are required.
 */
#ifndef MULIB_UNIT_H
#define MULIB_UNIT_H
#include "mulib-archml.h"
#include "mulib-emitter.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_unit_family {
  MULIB_UNIT_MPU = 1, MULIB_UNIT_MCU, MULIB_UNIT_GPU
} mulib_unit_family;
/** Owned, immutable after initialization. Queries borrow from document. */
typedef struct mulib_unit {
  mulib_archml_t *document;
  mulib_unit_family family;
  mulib_emitter_order order;
  unsigned bits;
} mulib_unit_t;
/** Load a builtin by public name (aarch64 maps to ArchML arm64). Success is
 * MULIB_ARCHML_OK (1). Failure leaves context unchanged. Rejects live contexts,
 * unknown names and family mismatches. Error is optional and always refreshed. */
mulib_archml_result mulib_unit_init (mulib_unit_t *, mulib_unit_family,
    const char *name, mulib_archml_error_t *);
/** Load a future/custom profile into the explicitly selected family. Source is
 * copied; ArchML limits apply. Names need not belong to the builtin inventory.
 * Target width must be 1..64. This assigns a family, not hardware certification. */
mulib_archml_result mulib_unit_parse (mulib_unit_t *, mulib_unit_family,
    const char *, size_t, const mulib_archml_options_t *, mulib_archml_error_t *);
/** Release ownership, clear fields; NULL and repeated free are harmless. */
void mulib_unit_free (mulib_unit_t *);
/** Borrow the profile, or NULL for a NULL/uninitialized context. */
const mulib_archml_t *mulib_unit_document (const mulib_unit_t *);
/** Exact record lookup. Borrowed result survives until free. */
const mulib_archml_node_t *mulib_unit_find (const mulib_unit_t *,
    const char *kind, const char *name);
/** Resolve register aliases, encoding and overlapping-storage metadata. */
int mulib_unit_register_info (const mulib_unit_t *, const char *,
    mulib_archml_register_t *);
/** Check an instruction/operation's explicit feature set; 1/0/-1 as ArchML. */
int mulib_unit_enabled (const mulib_unit_t *, const char *name,
    const char *const *features, size_t count);
/** Encode a concrete form with numeric operands in profile order. Signed
 * operands are uint64 two's-complement patterns, displacements are in bytes.
 * Features/modes are checked separately. All outputs unchanged on failure. */
mulib_archml_result mulib_unit_encode (const mulib_unit_t *, const char *,
    const uint64_t *, size_t, void *, size_t, size_t *);
/** Decode a named concrete form; no instruction auto-detection. Transactional. */
mulib_archml_result mulib_unit_decode (const mulib_unit_t *, const char *,
    const void *, size_t, uint64_t *, size_t, size_t *);
/** Append one encoded form. Requires matching target byte order; no automatic
 * alignment, labels or relocations. Failure preserves emitter bytes and size. */
mulib_archml_result mulib_unit_emit (const mulib_unit_t *, mulib_emitter_t *,
    const char *, const uint64_t *, size_t, mulib_archml_error_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_UNIT) && !defined(MULIB_UNIT_IMPLEMENTED)
#define MULIB_UNIT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_unit_builtin_ {
  const char *name, *profile;
  mulib_unit_family family;
  const char *const *source;
} mulib_unit_builtin_;
#include "mulib-unit-data.inc"
static mulib_archml_result
mulib_unit_error_ (mulib_archml_error_t *e, mulib_archml_result code,
                   const char *message)
{
  if (e) {
    e->code = code; e->offset = 0; e->line = e->column = 1;
    e->message = message;
  }
  return code;
}
static int mulib_unit_family_ (mulib_unit_family family)
{ return family == MULIB_UNIT_MPU || family == MULIB_UNIT_MCU || family == MULIB_UNIT_GPU; }
mulib_archml_result
mulib_unit_parse (mulib_unit_t *c, mulib_unit_family family, const char *source,
    size_t length, const mulib_archml_options_t *options, mulib_archml_error_t *e)
{
  mulib_unit_t next = {0};
  const mulib_archml_node_t *target, *endian;
  uint64_t bits;
  mulib_archml_result rc;
  if (!c || c->document || !mulib_unit_family_ (family))
    return mulib_unit_error_ (e, MULIB_ARCHML_INVALID, "invalid or live unit context");
  rc = mulib_archml_parse (source, length, options, &next.document, e);
  if (rc != MULIB_ARCHML_OK) return rc;
  target = mulib_archml_at (next.document, "target", 0);
  endian = mulib_archml_child (mulib_archml_property (target, "endian"), 1);
  if (!mulib_archml_u64 (mulib_archml_child (mulib_archml_property (target, "bits"), 1), &bits)
      || !bits || bits > 64 || !endian || !endian->text
      || (strcmp (endian->text, "little") && strcmp (endian->text, "big"))) {
    mulib_archml_destroy (next.document);
    return mulib_unit_error_ (e, MULIB_ARCHML_SCHEMA, "unit requires width 1..64 and explicit endian");
  }
  next.family = family; next.bits = (unsigned)bits;
  next.order = !strcmp (endian->text, "little") ? MULIB_EMITTER_LITTLE : MULIB_EMITTER_BIG;
  *c = next;
  return mulib_unit_error_ (e, MULIB_ARCHML_OK, NULL);
}
mulib_archml_result
mulib_unit_init (mulib_unit_t *c, mulib_unit_family family, const char *name,
                 mulib_archml_error_t *e)
{
  const mulib_unit_builtin_ *b = NULL;
  size_t length = 0, at = 0;
  char *source;
  mulib_archml_result rc;
  if (!c || c->document || !name || !mulib_unit_family_ (family))
    return mulib_unit_error_ (e, MULIB_ARCHML_INVALID, "invalid or live unit context");
  for (size_t i = 0; i < sizeof (mulib_unit_builtins_) / sizeof (*mulib_unit_builtins_); ++i)
    if (!strcmp (name, mulib_unit_builtins_[i].name)) { b = &mulib_unit_builtins_[i]; break; }
  if (!b || b->family != family)
    return mulib_unit_error_ (e, MULIB_ARCHML_MISMATCH, "unknown target or wrong unit family");
  for (size_t i = 0; b->source[i]; ++i) {
    size_t n = strlen (b->source[i]);
    if (n > SIZE_MAX - length - 1)
      return mulib_unit_error_ (e, MULIB_ARCHML_LIMIT, "profile size overflow");
    length += n;
  }
  source = (char *)malloc (length + 1);
  if (!source) return mulib_unit_error_ (e, MULIB_ARCHML_NOMEM, "profile allocation failed");
  for (size_t i = 0; b->source[i]; ++i) {
    size_t n = strlen (b->source[i]); memcpy (source + at, b->source[i], n); at += n;
  }
  source[length] = 0;
  rc = mulib_unit_parse (c, family, source, length, NULL, e);
  free (source);
  if (rc == MULIB_ARCHML_OK && strcmp (mulib_archml_name (c->document), b->profile)) {
    mulib_unit_free (c);
    return mulib_unit_error_ (e, MULIB_ARCHML_SCHEMA, "builtin profile identity mismatch");
  }
  return rc;
}
void mulib_unit_free (mulib_unit_t *c)
{
  if (c) { mulib_unit_t empty = {0}; mulib_archml_destroy (c->document); *c = empty; }
}
const mulib_archml_t *mulib_unit_document (const mulib_unit_t *c)
{ return c ? c->document : NULL; }
const mulib_archml_node_t *mulib_unit_find (const mulib_unit_t *c, const char *kind, const char *name)
{ return mulib_archml_find (mulib_unit_document (c), kind, name); }
int mulib_unit_register_info (const mulib_unit_t *c, const char *name, mulib_archml_register_t *out)
{ return mulib_archml_register_info (mulib_unit_document (c), name, out); }
int mulib_unit_enabled (const mulib_unit_t *c, const char *name, const char *const *features, size_t count)
{
  const mulib_archml_node_t *n = mulib_unit_find (c, "instruction", name);
  if (!n) n = mulib_unit_find (c, "operation", name);
  if (!n) return -1;
  return mulib_archml_enabled (mulib_unit_document (c), n, features, count);
}
mulib_archml_result mulib_unit_encode (const mulib_unit_t *c, const char *name,
    const uint64_t *values, size_t count, void *bytes, size_t capacity, size_t *length)
{
  if (!c || !c->document) return MULIB_ARCHML_INVALID;
  if (!name) return MULIB_ARCHML_INVALID;
  if (!mulib_unit_find (c, "instruction", name)) return MULIB_ARCHML_MISMATCH;
  return mulib_archml_encode (c->document, name, values, count, bytes, capacity, length);
}
mulib_archml_result mulib_unit_decode (const mulib_unit_t *c, const char *name,
    const void *bytes, size_t length, uint64_t *values, size_t count, size_t *consumed)
{
  if (!c || !c->document || !name) return MULIB_ARCHML_INVALID;
  if (!mulib_unit_find (c, "instruction", name)) return MULIB_ARCHML_MISMATCH;
  return mulib_archml_decode (c->document, name, bytes, length, values, count, consumed);
}
mulib_archml_result mulib_unit_emit (const mulib_unit_t *c, mulib_emitter_t *image,
    const char *name, const uint64_t *values, size_t count, mulib_archml_error_t *e)
{
  unsigned char bytes[MULIB_ARCHML_MAX_BYTES];
  size_t length = 0;
  mulib_archml_result rc;
  if (!c || !c->document || !image || image->order != c->order
      || image->size > image->capacity || (!image->data && image->capacity))
    return mulib_unit_error_ (e, MULIB_ARCHML_INVALID, "invalid context, image or target byte order");
  rc = mulib_unit_encode (c, name, values, count, bytes, sizeof (bytes), &length);
  if (rc != MULIB_ARCHML_OK)
    return mulib_unit_error_ (e, rc, "unsupported form or invalid operands");
  if (length > image->capacity - image->size)
    return mulib_unit_error_ (e, MULIB_ARCHML_LIMIT, "image capacity exceeded");
  (void)mulib_emitter_bytes (image, bytes, length);
  return mulib_unit_error_ (e, MULIB_ARCHML_OK, NULL);
}
#ifdef __cplusplus
}
#endif
#endif
