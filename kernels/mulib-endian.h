/**
 * @file mulib-endian.h
 * @brief Explicit little- and big-endian primitive load/store helpers.
 * @defgroup mulib_endian MuLIB endian
 * @{
 * @details Safe on unaligned byte buffers; unlike pointer casts these
 * functions avoid alignment and strict-aliasing violations.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ENDIAN before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ENDIAN_H
#define MULIB_ENDIAN_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief mulib endian load le32; see manual/mulib-endian.md for contracts
   * and examples. */
  uint32_t mulib_endian_load_le32 (const unsigned char *bytes);
  /** @brief mulib endian load be32; see manual/mulib-endian.md for contracts
   * and examples. */
  uint32_t mulib_endian_load_be32 (const unsigned char *bytes);
  /** @brief mulib endian store le32; see manual/mulib-endian.md for contracts
   * and examples. */
  void mulib_endian_store_le32 (unsigned char *bytes, uint32_t value);
  /** @brief mulib endian store be32; see manual/mulib-endian.md for contracts
   * and examples. */
  void mulib_endian_store_be32 (unsigned char *bytes, uint32_t value);
  /** Unaligned 64-bit counterparts; NULL loads zero and stores do nothing. */
  uint64_t mulib_endian_load_le64 (const unsigned char *bytes);
  uint64_t mulib_endian_load_be64 (const unsigned char *bytes);
  void mulib_endian_store_le64 (unsigned char *bytes, uint64_t value);
  void mulib_endian_store_be64 (unsigned char *bytes, uint64_t value);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ENDIAN_H */

#if defined(MULIB_IMPL_ENDIAN) && !defined(MULIB_ENDIAN_IMPLEMENTED)
#define MULIB_ENDIAN_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  uint32_t
  mulib_endian_load_le32 (const unsigned char *b)
  {
    return b ? (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16
                   | (uint32_t)b[3] << 24
             : 0;
  }
  uint32_t
  mulib_endian_load_be32 (const unsigned char *b)
  {
    return b ? (uint32_t)b[3] | (uint32_t)b[2] << 8 | (uint32_t)b[1] << 16
                   | (uint32_t)b[0] << 24
             : 0;
  }
  void
  mulib_endian_store_le32 (unsigned char *b, uint32_t v)
  {
    if (b)
      for (unsigned i = 0; i < 4; i++)
        b[i] = (unsigned char)(v >> (i * 8));
  }
  void
  mulib_endian_store_be32 (unsigned char *b, uint32_t v)
  {
    if (b)
      for (unsigned i = 0; i < 4; i++)
        b[i] = (unsigned char)(v >> ((3 - i) * 8));
  }
  uint64_t mulib_endian_load_le64 (const unsigned char *b)
  { return b ? (uint64_t)mulib_endian_load_le32 (b) | (uint64_t)mulib_endian_load_le32 (b + 4) << 32 : 0; }
  uint64_t mulib_endian_load_be64 (const unsigned char *b)
  { return b ? (uint64_t)mulib_endian_load_be32 (b) << 32 | mulib_endian_load_be32 (b + 4) : 0; }
  void mulib_endian_store_le64 (unsigned char *b, uint64_t v)
  { if (b) { mulib_endian_store_le32 (b, (uint32_t)v); mulib_endian_store_le32 (b + 4, (uint32_t)(v >> 32)); } }
  void mulib_endian_store_be64 (unsigned char *b, uint64_t v)
  { if (b) { mulib_endian_store_be32 (b, (uint32_t)(v >> 32)); mulib_endian_store_be32 (b + 4, (uint32_t)v); } }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
