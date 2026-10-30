/**
 * @file mulib-bycutils.h
 * @brief Checked byte-code arithmetic and unaligned little-endian integers.
 * Define MULIB_IMPL_BYCUTILS once. No native structure casts or allocation.
 */
#ifndef MULIB_BYCUTILS_H
#define MULIB_BYCUTILS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Check a range without overflowing offset+length. */
  int mulib_bycutils_range (size_t size, size_t offset, size_t length);
  /** @brief Round up to a nonzero power-of-two alignment; unchanged on
   * failure. */
  int mulib_bycutils_align (size_t value, size_t alignment, size_t *out);
  /** @brief Read 1..8 little-endian bytes; unchanged on invalid range/width.
   */
  int mulib_bycutils_load (const void *data, size_t size, size_t offset,
                           unsigned width, uint64_t *out);
  /** @brief Store 1..8 bytes; reject a value that does not fit, without
   * mutation. */
  int mulib_bycutils_store (void *data, size_t size, size_t offset,
                            unsigned width, uint64_t value);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BYCUTILS) && !defined(MULIB_BYCUTILS_IMPLEMENTED)
#define MULIB_BYCUTILS_IMPLEMENTED
int
mulib_bycutils_range (size_t n, size_t o, size_t z)
{
  return o <= n && z <= n - o;
}
int
mulib_bycutils_align (size_t n, size_t a, size_t *out)
{
  if (!out || !a || (a & (a - 1)) || n > SIZE_MAX - (a - 1))
    return 0;
  *out = (n + a - 1) & ~(a - 1);
  return 1;
}
int
mulib_bycutils_load (const void *data, size_t n, size_t o, unsigned w,
                     uint64_t *out)
{
  const unsigned char *p = (const unsigned char *)data;
  uint64_t v = 0;
  unsigned i;
  if (!p || !out || !w || w > 8 || !mulib_bycutils_range (n, o, w))
    return 0;
  for (i = 0; i < w; ++i)
    v |= (uint64_t)p[o + i] << (8 * i);
  *out = v;
  return 1;
}
int
mulib_bycutils_store (void *data, size_t n, size_t o, unsigned w, uint64_t v)
{
  unsigned char *p = (unsigned char *)data;
  unsigned i;
  if (!p || !w || w > 8 || !mulib_bycutils_range (n, o, w)
      || (w < 8 && (v >> (8 * w))))
    return 0;
  for (i = 0; i < w; ++i)
    p[o + i] = (unsigned char)(v >> (8 * i));
  return 1;
}
#endif
