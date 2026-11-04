/** @file mulib-pebuilder.h
 * @brief Minimal deterministic fixed-base AMD64 PE32+ console image.
 * Define MULIB_IMPL_PEBUILDER plus OBJFILEBUILDER, OBJFILEWRITER,
 * BYCUTILS, BINFILEWRITER and BUFFER.
 * Single read/execute .text, no imports, relocations, unwind tables or signing.
 * Caller supplies freestanding position-independent code, not a C runtime.
 * Atomic append, 1 success/0 invalid/OOM/overflow. */
#ifndef MULIB_PEBUILDER_H
#define MULIB_PEBUILDER_H
#include "mulib-binfilewriter.h"
#include "mulib-bycutils.h"
#include "mulib-objfilebuilder.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_pebuilder_text (mulib_buffer_t *out, const void *code,
                            size_t length, size_t entry_offset);
  typedef mulib_objfilebuilder_t mulib_pebuilder_t;
  /** @brief Copy a 1..8-byte named section; power-of-two alignment <=4096,
   * flags WRITE/ALLOC/EXEC = 1/2/4. Zero initialize builder. */
  int mulib_pebuilder_add (mulib_pebuilder_t *b, const char *name,
      const void *data, size_t size, size_t alignment, uint64_t flags);
  /** @brief Append AMD64 PE32+ data image with no entry/imports/signature.
   * machine must be 0x8664. Use text() for a freestanding executable. */
  int mulib_pebuilder_finish (const mulib_pebuilder_t *b, uint16_t machine, mulib_buffer_t *out);
  /** @brief Release all builder storage. */
  void mulib_pebuilder_free (mulib_pebuilder_t *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PEBUILDER) && !defined(MULIB_PEBUILDER_IMPLEMENTED)
#define MULIB_PEBUILDER_IMPLEMENTED
#include <string.h>
int mulib_pebuilder_add (mulib_pebuilder_t *b, const char *name,
    const void *p, size_t n, size_t a, uint64_t flags)
{
  if (!name || *name == '/' || strlen (name) > 8 || a > 4096 || n > UINT32_MAX) return 0;
  return mulib_objfilebuilder_add (b, name, p, n, a, flags);
}
void mulib_pebuilder_free (mulib_pebuilder_t *b) { mulib_objfilebuilder_free (b); }
int mulib_pebuilder_finish (const mulib_pebuilder_t *b, uint16_t machine, mulib_buffer_t *out)
{
  mulib_buffer_t tmp = { 0 };
  unsigned char h[512] = { 0 };
  size_t i, headers;
  uint64_t rva = 4096, code_size = 0, data_size = 0;
  int ok = 0;
  if (!b || !out || machine != 0x8664 || b->count > 96 || (!b->sections && b->count)) return 0;
  headers = (392 + b->count * 40 + 511) & ~(size_t)511;
  if (!mulib_pebuilder_text (&tmp, "\xc3", 1, 0)) goto done;
  memcpy (h, tmp.data, 392); mulib_buffer_free (&tmp);
  mulib_bycutils_store (h, 512, 134, 2, b->count);
  mulib_bycutils_store (h, 512, 150, 2, 0x2023); /* data-only DLL profile */
  mulib_bycutils_store (h, 512, 156, 4, 0); mulib_bycutils_store (h, 512, 168, 4, 0);
  mulib_bycutils_store (h, 512, 172, 4, 0); mulib_bycutils_store (h, 512, 212, 4, headers);
  if (!mulib_buffer_append (&tmp, h, 392)) goto done;
  memset (h, 0, sizeof (h));
  while (tmp.length < headers) {
    size_t n = headers - tmp.length; if (n > sizeof (h)) n = sizeof (h);
    if (!mulib_buffer_append (&tmp, h, n)) goto done;
  }
  rva = (headers + 4095) & ~(uint64_t)4095;
  for (i = 0; i < b->count; ++i) {
    const mulib_objfilewriter_section_t *s = b->sections + i;
    size_t raw;
    uint32_t flags = 0x40000040u;
    if (!s->name || !*s->name || *s->name == '/' || strlen (s->name) > 8 || !s->alignment || s->alignment > 4096
        || (s->alignment & (s->alignment - 1)) || (s->flags & ~UINT64_C (7))
        || (!s->data && s->size) || s->size > UINT32_MAX - 4095
        || !mulib_bycutils_align (s->size, 512, &raw) || tmp.length > UINT32_MAX
        || raw > UINT32_MAX - tmp.length || rva + s->size + 4095 > UINT32_MAX) goto done;
    if (s->flags & 1) flags |= 0x80000000u;
    if (s->flags & 4) flags = (flags & ~0x40u) | 0x20000020u;
    if (s->flags & 4) code_size += raw; else data_size += raw;
    memset (h, 0, 40); memcpy (h, s->name, strlen (s->name));
    mulib_bycutils_store (h, 40, 8, 4, s->size); mulib_bycutils_store (h, 40, 12, 4, rva);
    mulib_bycutils_store (h, 40, 16, 4, raw); mulib_bycutils_store (h, 40, 20, 4, raw ? tmp.length : 0);
    mulib_bycutils_store (h, 40, 36, 4, flags); memcpy (tmp.data + 392 + i * 40, h, 40);
    if (!mulib_buffer_append (&tmp, s->data, s->size) || !mulib_binfilewriter_align (&tmp, 512)) goto done;
    rva += (s->size + 4095) & ~(uint64_t)4095;
  }
  mulib_binfilewriter_patch (&tmp, 208, 4, rva);
  mulib_binfilewriter_patch (&tmp, 156, 4, code_size);
  mulib_binfilewriter_patch (&tmp, 160, 4, data_size);
  ok = mulib_buffer_append (out, tmp.data, tmp.length);
done:
  mulib_buffer_free (&tmp); return ok;
}
int
mulib_pebuilder_text (mulib_buffer_t *out, const void *p, size_t n,
                      size_t entry)
{
  unsigned char h[512] = { 0 };
  size_t raw, image;
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || !p || !n || entry >= n || n > UINT32_MAX - 8192
      || !mulib_bycutils_align (n, 512, &raw)
      || !mulib_bycutils_align (n, 4096, &image))
    return 0;
  h[0] = 'M';
  h[1] = 'Z';
  h[128] = 'P';
  h[129] = 'E';
  mulib_bycutils_store (h, sizeof (h), 60, 4, 128);
  mulib_bycutils_store (h, sizeof (h), 132, 2, 0x8664);
  mulib_bycutils_store (h, sizeof (h), 134, 2, 1);
  mulib_bycutils_store (h, sizeof (h), 148, 2, 240);
  mulib_bycutils_store (h, sizeof (h), 150, 2, 0x23);
  mulib_bycutils_store (h, sizeof (h), 152, 2, 0x20b);
  h[154] = 1;
  mulib_bycutils_store (h, sizeof (h), 156, 4, raw);
  mulib_bycutils_store (h, sizeof (h), 168, 4, 4096 + entry);
  mulib_bycutils_store (h, sizeof (h), 172, 4, 4096);
  mulib_bycutils_store (h, sizeof (h), 176, 8, UINT64_C (0x140000000));
  mulib_bycutils_store (h, sizeof (h), 184, 4, 4096);
  mulib_bycutils_store (h, sizeof (h), 188, 4, 512);
  mulib_bycutils_store (h, sizeof (h), 192, 2, 6);
  mulib_bycutils_store (h, sizeof (h), 200, 2, 6);
  mulib_bycutils_store (h, sizeof (h), 208, 4, image + 4096);
  mulib_bycutils_store (h, sizeof (h), 212, 4, 512);
  mulib_bycutils_store (h, sizeof (h), 220, 2, 3);
  mulib_bycutils_store (h, sizeof (h), 222, 2, 0x100);
  mulib_bycutils_store (h, sizeof (h), 224, 8, 1048576);
  mulib_bycutils_store (h, sizeof (h), 232, 8, 4096);
  mulib_bycutils_store (h, sizeof (h), 240, 8, 1048576);
  mulib_bycutils_store (h, sizeof (h), 248, 8, 4096);
  mulib_bycutils_store (h, sizeof (h), 260, 4, 16);
  memcpy (h + 392, ".text", 5);
  mulib_bycutils_store (h, sizeof (h), 400, 4, n);
  mulib_bycutils_store (h, sizeof (h), 404, 4, 4096);
  mulib_bycutils_store (h, sizeof (h), 408, 4, raw);
  mulib_bycutils_store (h, sizeof (h), 412, 4, 512);
  mulib_bycutils_store (h, sizeof (h), 428, 4, 0x60000020);
  ok = mulib_buffer_append (&b, h, sizeof (h)) && mulib_buffer_append (&b, p, n)
       && mulib_binfilewriter_align (&b, 512)
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
#endif
