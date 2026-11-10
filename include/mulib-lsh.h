/* MuLIB: Band-key construction for locality-sensitive MinHash candidate retrieval. C11
 * single-header; define MULIB_IMPL_LSH once. */
#ifndef MULIB_LSH_H
#define MULIB_LSH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  /* Deterministic hash of signature elements in band [offset,offset+count). */
  uint64_t mulib_lsh_band (const uint64_t *signature, size_t offset, size_t count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LSH) && !defined(MULIB_LSH_IMPLEMENTED)
#define MULIB_LSH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t
  mulib_lsh_band (const uint64_t *s, size_t offset, size_t n)
  {
    if (!s || n > SIZE_MAX - offset)
      return 0;
    uint64_t h = UINT64_C (14695981039346656037);
    for (size_t i = 0; i < n; i++)
      {
        uint64_t x = s[offset + i];
        for (unsigned b = 0; b < 8; b++)
          {
            h ^= (unsigned char)(x >> (b * 8));
            h *= UINT64_C (1099511628211);
          }
      }
    h ^= (uint64_t)n;
    return h;
  }
#ifdef __cplusplus
}
#endif
#endif
