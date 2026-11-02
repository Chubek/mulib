/** @file mulib-coffbuilder.h
 * @brief Deterministic AMD64 COFF object with one read/execute .text section.
 * Define MULIB_IMPL_COFFBUILDER, BYCUTILS and BUFFER. No symbols/relocations.
 * Atomic append; input may alias output; 1 success, 0 invalid/OOM/overflow. */
#ifndef MULIB_COFFBUILDER_H
#define MULIB_COFFBUILDER_H
#include "mulib-buffer.h"
#include "mulib-bycutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_coffbuilder_text (mulib_buffer_t *out, const void *code,
                              size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COFFBUILDER) && !defined(MULIB_COFFBUILDER_IMPLEMENTED)
#define MULIB_COFFBUILDER_IMPLEMENTED
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
