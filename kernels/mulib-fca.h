/* MuLIB: Formal Concept Analysis binary contexts up to 64 objects and 64 attributes.
 * C11 single-header; define MULIB_IMPL_FCA once. */
#ifndef MULIB_FCA_H
#define MULIB_FCA_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    size_t objects, attributes;
    uint64_t rows[64];
  } mulib_fca_t;
  int mulib_fca_init (mulib_fca_t *c, size_t objects, size_t attributes);
  int mulib_fca_set (mulib_fca_t *c, size_t object, size_t attribute, int present);
  uint64_t mulib_fca_extent (const mulib_fca_t *c, uint64_t attributes);
  uint64_t mulib_fca_intent (const mulib_fca_t *c, uint64_t objects);
  uint64_t mulib_fca_closure (const mulib_fca_t *c, uint64_t attributes);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FCA) && !defined(MULIB_FCA_IMPLEMENTED)
#define MULIB_FCA_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_fca_mask_ (size_t n)
  {
    return n == 64 ? UINT64_MAX : (n ? ((UINT64_C (1) << n) - 1) : 0);
  }
  int
  mulib_fca_init (mulib_fca_t *c, size_t o, size_t a)
  {
    if (!c || o > 64 || a > 64)
      return 0;
    memset (c, 0, sizeof (*c));
    c->objects = o;
    c->attributes = a;
    return 1;
  }
  int
  mulib_fca_set (mulib_fca_t *c, size_t o, size_t a, int present)
  {
    if (!c || c->objects > 64 || c->attributes > 64 || o >= c->objects
        || a >= c->attributes)
      return 0;
    if (present)
      c->rows[o] |= UINT64_C (1) << a;
    else
      c->rows[o] &= ~(UINT64_C (1) << a);
    return 1;
  }
  uint64_t
  mulib_fca_extent (const mulib_fca_t *c, uint64_t attrs)
  {
    if (!c || c->attributes > 64 || c->objects > 64)
      return 0;
    attrs &= mulib_fca_mask_ (c->attributes);
    uint64_t result = 0;
    for (size_t i = 0; i < c->objects; i++)
      if ((c->rows[i] & attrs) == attrs)
        result |= UINT64_C (1) << i;
    return result;
  }
  uint64_t
  mulib_fca_intent (const mulib_fca_t *c, uint64_t objects)
  {
    if (!c || c->attributes > 64 || c->objects > 64)
      return 0;
    uint64_t mask = mulib_fca_mask_ (c->attributes),
             set = objects & mulib_fca_mask_ (c->objects);
    for (size_t i = 0; i < c->objects; i++)
      if (set & (UINT64_C (1) << i))
        mask &= c->rows[i];
    return mask;
  }
  uint64_t
  mulib_fca_closure (const mulib_fca_t *c, uint64_t attrs)
  {
    return mulib_fca_intent (c, mulib_fca_extent (c, attrs));
  }
#ifdef __cplusplus
}
#endif
#endif
