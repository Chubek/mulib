/* MuLIB: Morton Z-order encoding/decoding for 2D unsigned 32-bit coordinates. C11
 * single-header; define MULIB_IMPL_ZORDER once. */
#ifndef MULIB_ZORDER_H
#define MULIB_ZORDER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t mulib_zorder_encode (uint32_t x, uint32_t y);
  void mulib_zorder_decode (uint64_t key, uint32_t *x, uint32_t *y);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ZORDER) && !defined(MULIB_ZORDER_IMPLEMENTED)
#define MULIB_ZORDER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t
  mulib_zorder_encode (uint32_t x, uint32_t y)
  {
    uint64_t v = 0;
    for (unsigned i = 0; i < 32; i++)
      {
        v |= ((uint64_t)((x >> i) & 1)) << (2 * i);
        v |= ((uint64_t)((y >> i) & 1)) << (2 * i + 1);
      }
    return v;
  }
  void
  mulib_zorder_decode (uint64_t key, uint32_t *x, uint32_t *y)
  {
    uint32_t a = 0, b = 0;
    for (unsigned i = 0; i < 32; i++)
      {
        a |= (uint32_t)((key >> (2 * i)) & 1) << i;
        b |= (uint32_t)((key >> (2 * i + 1)) & 1) << i;
      }
    if (x)
      *x = a;
    if (y)
      *y = b;
  }
#ifdef __cplusplus
}
#endif
#endif
