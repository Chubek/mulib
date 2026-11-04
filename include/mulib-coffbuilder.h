/** @file mulib-coffbuilder.h
 * @brief Deterministic AMD64 COFF object with one read/execute .text section.
 * Define MULIB_IMPL_COFFBUILDER plus OBJFILEBUILDER, OBJFILEWRITER,
 * BINFILEWRITER, BYCUTILS and BUFFER. No symbols/relocations.
 * Atomic append; input may alias output; 1 success, 0 invalid/OOM/overflow. */
#ifndef MULIB_COFFBUILDER_H
#define MULIB_COFFBUILDER_H
#include "mulib-buffer.h"
#include "mulib-bycutils.h"
#include "mulib-objfilebuilder.h"
#include "mulib-binfilewriter.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_coffbuilder_text (mulib_buffer_t *out, const void *code,
                              size_t length);
  typedef mulib_objfilebuilder_t mulib_coffbuilder_t;
  /** @brief Copy a section with a 1..8 byte name, alignment <=8192 and
   * WRITE/ALLOC/EXEC flags 1/2/4. Initialize builder to zero. */
  int mulib_coffbuilder_add (mulib_coffbuilder_t *b, const char *name,
      const void *data, size_t size, size_t alignment, uint64_t flags);
  /** @brief Atomically append a multi-section COFF object; no symbols/relocations. */
  int mulib_coffbuilder_finish (const mulib_coffbuilder_t *b, uint16_t machine, mulib_buffer_t *out);
  /** @brief Free owned names/bytes and reset; NULL allowed. */
  void mulib_coffbuilder_free (mulib_coffbuilder_t *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COFFBUILDER) && !defined(MULIB_COFFBUILDER_IMPLEMENTED)
#define MULIB_COFFBUILDER_IMPLEMENTED
int mulib_coffbuilder_add (mulib_coffbuilder_t *b, const char *name,
    const void *p, size_t n, size_t a, uint64_t flags)
{
  if (!name || *name == '/' || strlen (name) > 8 || a > 8192 || n > UINT32_MAX) return 0;
  return mulib_objfilebuilder_add (b, name, p, n, a, flags);
}
void mulib_coffbuilder_free (mulib_coffbuilder_t *b) { mulib_objfilebuilder_free (b); }
int mulib_coffbuilder_finish (const mulib_coffbuilder_t *b, uint16_t machine, mulib_buffer_t *out)
{
  unsigned char h[40] = { 0 };
  mulib_buffer_t tmp = { 0 };
  size_t i;
  int ok = 0;
  if (!b || !out || !machine || b->count > 32767 || (!b->sections && b->count)) return 0;
  mulib_bycutils_store (h, 40, 0, 2, machine);
  mulib_bycutils_store (h, 40, 2, 2, b->count);
  if (!mulib_buffer_append (&tmp, h, 20)) goto done;
  memset (h, 0, 40);
  for (i = 0; i < b->count; ++i) if (!mulib_buffer_append (&tmp, h, 40)) goto done;
  for (i = 0; i < b->count; ++i) {
    const mulib_objfilewriter_section_t *s = b->sections + i;
    uint32_t flags = 0x40000040u;
    unsigned exponent = 1;
    size_t a;
    if (!s->name || !*s->name || *s->name == '/' || strlen (s->name) > 8 || !s->alignment || s->alignment > 8192
        || (s->alignment & (s->alignment - 1)) || (s->flags & ~UINT64_C (7))
        || s->size > UINT32_MAX || (!s->data && s->size)) goto done;
    if (!mulib_binfilewriter_align (&tmp, s->alignment) || tmp.length > UINT32_MAX
        || s->size > UINT32_MAX - tmp.length) goto done;
    for (a = s->alignment; a > 1; a >>= 1) ++exponent;
    flags |= (uint32_t)exponent << 20;
    if (s->flags & 1) flags |= 0x80000000u;
    if (s->flags & 4) flags = (flags & ~0x40u) | 0x20000020u;
    memset (h, 0, 40); memcpy (h, s->name, strlen (s->name));
    mulib_bycutils_store (h, 40, 16, 4, s->size);
    mulib_bycutils_store (h, 40, 20, 4, s->size ? tmp.length : 0);
    mulib_bycutils_store (h, 40, 36, 4, flags);
    memcpy (tmp.data + 20 + i * 40, h, 40);
    if (!mulib_buffer_append (&tmp, s->data, s->size)) goto done;
  }
  ok = mulib_buffer_append (out, tmp.data, tmp.length);
done:
  mulib_buffer_free (&tmp); return ok;
}
int
mulib_coffbuilder_text (mulib_buffer_t *out, const void *p, size_t n)
{
  unsigned char h[60] = { 0 };
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || (!p && n) || n > UINT32_MAX - 60)
    return 0;
  mulib_bycutils_store (h, sizeof (h), 0, 2, 0x8664);
  mulib_bycutils_store (h, sizeof (h), 2, 2, 1);
  memcpy (h + 20, ".text", 5);
  mulib_bycutils_store (h, sizeof (h), 36, 4, n);
  mulib_bycutils_store (h, sizeof (h), 40, 4, n ? 60 : 0);
  mulib_bycutils_store (h, sizeof (h), 56, 4,
                        0x60500020); /* 16-byte alignment */
  ok = mulib_buffer_append (&b, h, sizeof (h)) && mulib_buffer_append (&b, p, n)
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
#endif
