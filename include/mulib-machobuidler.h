/** @file mulib-machobuidler.h
 * @brief Deterministic x86-64 MH_OBJECT with one __TEXT,__text section.
 * Historical filename retained. Define MULIB_IMPL_MACHOBUIDLER, BYCUTILS and
 * BUFFER. No symbols/relocations/signatures; not a macOS executable.
 * Atomic append, 1 success/0 invalid, overflow or allocation failure. */
#ifndef MULIB_MACHOBUIDLER_H
#define MULIB_MACHOBUIDLER_H
#include "mulib-buffer.h"
#include "mulib-bycutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_machobuilder_text (mulib_buffer_t *out, const void *code,
                               size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MACHOBUIDLER) && !defined(MULIB_MACHOBUIDLER_IMPLEMENTED)
#define MULIB_MACHOBUIDLER_IMPLEMENTED
int
mulib_machobuilder_text (mulib_buffer_t *out, const void *p, size_t n)
{
  unsigned char h[192] = { 0 };
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || (!p && n) || n > UINT32_MAX - 192)
    return 0;
  mulib_bycutils_store (h, sizeof (h), 0, 4, 0xfeedfacf);
  mulib_bycutils_store (h, sizeof (h), 4, 4, 0x01000007);
  mulib_bycutils_store (h, sizeof (h), 8, 4, 3);
  mulib_bycutils_store (h, sizeof (h), 12, 4, 1);
  mulib_bycutils_store (h, sizeof (h), 16, 4, 1);
  mulib_bycutils_store (h, sizeof (h), 20, 4, 152);
  mulib_bycutils_store (h, sizeof (h), 32, 4, 0x19);
  mulib_bycutils_store (h, sizeof (h), 36, 4, 152);
  mulib_bycutils_store (h, sizeof (h), 64, 8, n);
  mulib_bycutils_store (h, sizeof (h), 72, 8, 192);
  mulib_bycutils_store (h, sizeof (h), 80, 8, n);
  mulib_bycutils_store (h, sizeof (h), 88, 4, 7);
  mulib_bycutils_store (h, sizeof (h), 92, 4, 5);
  mulib_bycutils_store (h, sizeof (h), 96, 4, 1);
  memcpy (h + 104, "__text", 6);
  memcpy (h + 120, "__TEXT", 6);
  mulib_bycutils_store (h, sizeof (h), 144, 8, n);
  mulib_bycutils_store (h, sizeof (h), 152, 4, 192);
  mulib_bycutils_store (h, sizeof (h), 156, 4, 4);
  mulib_bycutils_store (h, sizeof (h), 168, 4, 0x80000400);
  ok = mulib_buffer_append (&b, h, sizeof (h)) && mulib_buffer_append (&b, p, n)
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
#endif
