/* MuLIB: Stable unsigned 64-bit document IDs and order-preserving big-endian
 * serialization. C11 single-header; define MULIB_IMPL_DOCID once. */
#ifndef MULIB_DOCID_H
#define MULIB_DOCID_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef uint64_t mulib_docid_t;
  void mulib_docid_encode (uint64_t id, unsigned char out[8]);
  uint64_t mulib_docid_decode (const unsigned char in[8]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DOCID) && !defined(MULIB_DOCID_IMPLEMENTED)
#define MULIB_DOCID_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_docid_encode (uint64_t id, unsigned char out[8])
  {
    if (!out)
      return;
    for (unsigned i = 0; i < 8; i++)
      out[i] = (unsigned char)(id >> (56 - 8 * i));
  }
  uint64_t
  mulib_docid_decode (const unsigned char in[8])
  {
    uint64_t v = 0;
    if (!in)
      return 0;
    for (unsigned i = 0; i < 8; i++)
      v = (v << 8) | in[i];
    return v;
  }
#ifdef __cplusplus
}
#endif
#endif
