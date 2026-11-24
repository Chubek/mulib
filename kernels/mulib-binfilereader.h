/** @file mulib-binfilereader.h
 * @brief Bounds-checked cursor over borrowed binary bytes.
 * Define MULIB_IMPL_BINFILEREADER plus BINFILE and BYCUTILS switches. */
#ifndef MULIB_BINFILEREADER_H
#define MULIB_BINFILEREADER_H
#include "mulib-binfile.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_binfilereader
  {
    mulib_binfile_t file;
    size_t position;
  } mulib_binfilereader_t;
  /** @brief Initialize cursor over borrowed input; 1 success. */
  int mulib_binfilereader_init (mulib_binfilereader_t *r, const void *data,
                                size_t size);
  /** @brief Read 1..8 little-endian bytes; cursor/output unchanged on failure.
   */
  int mulib_binfilereader_uint (mulib_binfilereader_t *r, unsigned width,
                                uint64_t *value);
  /** @brief Borrow and consume bytes; zero-size NULL span is valid. */
  int mulib_binfilereader_bytes (mulib_binfilereader_t *r, size_t size,
                                 mulib_binfile_t *out);
  /** @brief Absolute seek within [0,size]; 1 success. */
  int mulib_binfilereader_seek (mulib_binfilereader_t *r, size_t position);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BINFILEREADER)                                         \
    && !defined(MULIB_BINFILEREADER_IMPLEMENTED)
#define MULIB_BINFILEREADER_IMPLEMENTED
#include "mulib-bycutils.h"
int
mulib_binfilereader_init (mulib_binfilereader_t *r, const void *p, size_t n)
{
  if (!r || !mulib_binfile_open (&r->file, p, n))
    return 0;
  r->position = 0;
  return 1;
}
int
mulib_binfilereader_uint (mulib_binfilereader_t *r, unsigned w, uint64_t *v)
{
  if (!r
      || !mulib_bycutils_load (r->file.data, r->file.size, r->position, w, v))
    return 0;
  r->position += w;
  return 1;
}
int
mulib_binfilereader_bytes (mulib_binfilereader_t *r, size_t n,
                           mulib_binfile_t *out)
{
  if (!r || !mulib_binfile_slice (&r->file, r->position, n, out))
    return 0;
  r->position += n;
  return 1;
}
int
mulib_binfilereader_seek (mulib_binfilereader_t *r, size_t p)
{
  if (!r || p > r->file.size)
    return 0;
  r->position = p;
  return 1;
}
#endif
