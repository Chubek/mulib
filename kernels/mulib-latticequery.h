/* MuLIB: Attribute/extent predicate enumeration over concept-lattice results. C11
 * single-header; define MULIB_IMPL_LATTICEQUERY once. */
#ifndef MULIB_LATTICEQUERY_H
#define MULIB_LATTICEQUERY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-conceptlattice.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /* Return matching concept indices, SIZE_MAX for insufficient output capacity. */
  size_t mulib_latticequery_filter (const mulib_concept_t *concepts, size_t count,
                                    uint64_t all_attributes, uint64_t any_objects,
                                    size_t *indices, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LATTICEQUERY) && !defined(MULIB_LATTICEQUERY_IMPLEMENTED)
#define MULIB_LATTICEQUERY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_latticequery_filter (const mulib_concept_t *c, size_t n, uint64_t attrs,
                             uint64_t objects, size_t *out, size_t cap)
  {
    if ((!c && n) || (!out && cap))
      return SIZE_MAX;
    size_t count = 0;
    for (size_t i = 0; i < n; i++)
      if ((c[i].intent & attrs) == attrs && (!objects || (c[i].extent & objects)))
        count++;
    if (count > cap)
      return SIZE_MAX;
    for (size_t i = 0, j = 0; i < n; i++)
      if ((c[i].intent & attrs) == attrs && (!objects || (c[i].extent & objects)))
        out[j++] = i;
    return count;
  }
#ifdef __cplusplus
}
#endif
#endif
