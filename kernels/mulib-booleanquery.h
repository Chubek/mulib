/* MuLIB: Linear-time postings intersection/union/difference on sorted documents. C11
 * single-header; define MULIB_IMPL_BOOLEANQUERY once. */
#ifndef MULIB_BOOLEANQUERY_H
#define MULIB_BOOLEANQUERY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-postings.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_BOOLEAN_AND,
    MULIB_BOOLEAN_OR,
    MULIB_BOOLEAN_NOT
  } mulib_boolean_op_t;
  /* Writes a fresh output; requires zero initialized out distinct from inputs. */
  int mulib_booleanquery_apply (const mulib_postings_t *a, const mulib_postings_t *b,
                                mulib_boolean_op_t op, mulib_postings_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BOOLEANQUERY) && !defined(MULIB_BOOLEANQUERY_IMPLEMENTED)
#define MULIB_BOOLEANQUERY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_booleanquery_apply (const mulib_postings_t *a, const mulib_postings_t *b,
                            mulib_boolean_op_t op, mulib_postings_t *out)
  {
    if (!a || !b || !out || out == a || out == b || out->items || out->count
        || out->capacity || (unsigned)op > MULIB_BOOLEAN_NOT)
      return 0;
    size_t cap = a->count;
    if (op == MULIB_BOOLEAN_OR)
      {
        if (cap > SIZE_MAX - b->count)
          return 0;
        cap += b->count;
      }
    if (op == MULIB_BOOLEAN_AND && b->count < cap)
      cap = b->count;
    if (cap > SIZE_MAX / sizeof (*out->items))
      return 0;
    mulib_postings_t result = { 0 };
    if (cap)
      {
        result.items = (mulib_posting_t *)malloc (cap * sizeof (*result.items));
        if (!result.items)
          return 0;
        result.capacity = cap;
      }
    size_t i = 0, j = 0;
    while (i < a->count || j < b->count)
      {
        if (i == a->count && op != MULIB_BOOLEAN_OR)
          break;
        uint64_t id;
        uint32_t tf;
        int from_a = 0, from_b = 0;
        if (j == b->count
            || (i < a->count && a->items[i].document < b->items[j].document))
          {
            id = a->items[i].document;
            tf = a->items[i++].frequency;
            from_a = 1;
          }
        else if (i == a->count || b->items[j].document < a->items[i].document)
          {
            id = b->items[j].document;
            tf = b->items[j++].frequency;
            from_b = 1;
          }
        else
          {
            id = a->items[i].document;
            tf = a->items[i].frequency > b->items[j].frequency ? a->items[i].frequency
                                                               : b->items[j].frequency;
            i++;
            j++;
            from_a = from_b = 1;
          }
        if ((op == MULIB_BOOLEAN_AND && from_a && from_b) || op == MULIB_BOOLEAN_OR
            || (op == MULIB_BOOLEAN_NOT && from_a && !from_b))
          result.items[result.count++] = (mulib_posting_t){ id, tf };
      }
    *out = result;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
