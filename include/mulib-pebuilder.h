/** @file mulib-pebuilder.h
 * @brief Minimal deterministic fixed-base AMD64 PE32+ console image.
 * Define MULIB_IMPL_PEBUILDER, BYCUTILS, BINFILEWRITER and BUFFER.
 * Single read/execute .text, no imports, relocations, unwind tables or signing.
 * Caller supplies freestanding position-independent code, not a C runtime.
 * Atomic append, 1 success/0 invalid/OOM/overflow. */
#ifndef MULIB_PEBUILDER_H
#define MULIB_PEBUILDER_H
#include "mulib-binfilewriter.h"
#include "mulib-bycutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_pebuilder_text (mulib_buffer_t *out, const void *code,
                            size_t length, size_t entry_offset);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PEBUILDER) && !defined(MULIB_PEBUILDER_IMPLEMENTED)
#define MULIB_PEBUILDER_IMPLEMENTED
#include <string.h>
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
