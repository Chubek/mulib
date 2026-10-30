/** @file mulib-binfilewriter.h
 * @brief Binary integer and zero-padding writer over MuLIB buffers.
 * Define MULIB_IMPL_BINFILEWRITER plus BUFFER and BYCUTILS switches. */
#ifndef MULIB_BINFILEWRITER_H
#define MULIB_BINFILEWRITER_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append a 1..8 byte unsigned little-endian integer; 1 succeeds. */
  int mulib_binfilewriter_uint (mulib_buffer_t *out, unsigned width,
                                uint64_t value);
  /** @brief Append zero padding to power-of-two alignment, atomically. */
  int mulib_binfilewriter_align (mulib_buffer_t *out, size_t alignment);
  /** @brief Patch within existing length; 1 succeeds, otherwise unchanged. */
  int mulib_binfilewriter_patch (mulib_buffer_t *out, size_t offset,
                                 unsigned width, uint64_t value);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BINFILEWRITER)                                         \
    && !defined(MULIB_BINFILEWRITER_IMPLEMENTED)
#define MULIB_BINFILEWRITER_IMPLEMENTED
#include "mulib-bycutils.h"
int
mulib_binfilewriter_uint (mulib_buffer_t *b, unsigned w, uint64_t v)
{
  unsigned char p[8];
  return mulib_bycutils_store (p, 8, 0, w, v) && mulib_buffer_append (b, p, w);
}
int
mulib_binfilewriter_align (mulib_buffer_t *b, size_t a)
{
  size_t n;
  unsigned char *p;
  int ok;
  if (!b || !mulib_bycutils_align (b->length, a, &n))
    return 0;
  n -= b->length;
  if (!n)
    return 1;
  p = (unsigned char *)calloc (n, 1);
  if (!p)
    return 0;
  ok = mulib_buffer_append (b, p, n);
  free (p);
  return ok;
}
int
mulib_binfilewriter_patch (mulib_buffer_t *b, size_t o, unsigned w, uint64_t v)
{
  return b && mulib_bycutils_store (b->data, b->length, o, w, v);
}
#endif
