/* MuLIB: Enumerate exact FCA concept intent/extent pairs with bounded capacity. C11
 * single-header; define MULIB_IMPL_CONCEPTLATTICE once. */
#ifndef MULIB_CONCEPTLATTICE_H
#define MULIB_CONCEPTLATTICE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-nextclosure.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t intent, extent;
  } mulib_concept_t;
  /* Returns number of concepts, SIZE_MAX on insufficient capacity without output
   * mutation. */
  size_t mulib_conceptlattice_enumerate (const mulib_fca_t *c, mulib_concept_t *out,
                                         size_t capacity);
  /* a<=b in extent ordering, equivalently intent_b subset intent_a. */
  int mulib_conceptlattice_leq (mulib_concept_t a, mulib_concept_t b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONCEPTLATTICE) && !defined(MULIB_CONCEPTLATTICE_IMPLEMENTED)
#define MULIB_CONCEPTLATTICE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_conceptlattice_enumerate (const mulib_fca_t *c, mulib_concept_t *out,
                                  size_t cap)
  {
    if (!c || c->objects > 64 || c->attributes > 64 || (!out && cap))
      return SIZE_MAX;
    uint64_t a = mulib_nextclosure_first (c);
    size_t count = 0;
    for (;;)
      {
        if (count >= cap)
          return SIZE_MAX;
        count++;
        uint64_t next;
        if (!mulib_nextclosure_next (c, a, &next))
          break;
        a = next;
      }
    a = mulib_nextclosure_first (c);
    for (size_t i = 0; i < count; i++)
      {
        out[i] = (mulib_concept_t){ a, mulib_fca_extent (c, a) };
        if (i + 1 < count)
          {
            uint64_t next;
            mulib_nextclosure_next (c, a, &next);
            a = next;
          }
      }
    return count;
  }
  int
  mulib_conceptlattice_leq (mulib_concept_t a, mulib_concept_t b)
  {
    return (a.extent & b.extent) == a.extent;
  }
#ifdef __cplusplus
}
#endif
#endif
