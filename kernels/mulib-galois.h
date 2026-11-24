/* MuLIB: FCA Galois derivation aliases with extent and intent closure. C11
 * single-header; define MULIB_IMPL_GALOIS once. */
#ifndef MULIB_GALOIS_H
#define MULIB_GALOIS_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-fca.h"
#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t mulib_galois_objects (const mulib_fca_t *c, uint64_t attributes);
  uint64_t mulib_galois_attributes (const mulib_fca_t *c, uint64_t objects);
  uint64_t mulib_galois_object_closure (const mulib_fca_t *c, uint64_t objects);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GALOIS) && !defined(MULIB_GALOIS_IMPLEMENTED)
#define MULIB_GALOIS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t
  mulib_galois_objects (const mulib_fca_t *c, uint64_t a)
  {
    return mulib_fca_extent (c, a);
  }
  uint64_t
  mulib_galois_attributes (const mulib_fca_t *c, uint64_t o)
  {
    return mulib_fca_intent (c, o);
  }
  uint64_t
  mulib_galois_object_closure (const mulib_fca_t *c, uint64_t o)
  {
    return mulib_fca_extent (c, mulib_fca_intent (c, o));
  }
#ifdef __cplusplus
}
#endif
#endif
