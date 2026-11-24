/* MuLIB: Typed equality and ordering of MuLIB datum scalar values. C11 single-header;
 * define MULIB_IMPL_QUERYEXPR once. */
#ifndef MULIB_QUERYEXPR_H
#define MULIB_QUERYEXPR_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-datum.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /* -1 less, 0 equal, 1 greater, 2 incomparable (e.g. NULL or mixed types). */
  int mulib_queryexpr_compare (const mulib_datum_t *a, const mulib_datum_t *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_QUERYEXPR) && !defined(MULIB_QUERYEXPR_IMPLEMENTED)
#define MULIB_QUERYEXPR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_queryexpr_compare (const mulib_datum_t *a, const mulib_datum_t *b)
  {
    if (!a || !b || a->kind != b->kind || a->kind == MULIB_DATUM_NULL)
      return 2;
    switch (a->kind)
      {
      case MULIB_DATUM_I64:
        return (a->value.integer > b->value.integer)
               - (a->value.integer < b->value.integer);
      case MULIB_DATUM_F64:
        if (isnan (a->value.real) || isnan (b->value.real))
          return 2;
        return (a->value.real > b->value.real) - (a->value.real < b->value.real);
      case MULIB_DATUM_BOOL:
        return (a->value.boolean > b->value.boolean)
               - (a->value.boolean < b->value.boolean);
      case MULIB_DATUM_BYTES:
        {
          size_t n = a->value.bytes.length < b->value.bytes.length
                         ? a->value.bytes.length
                         : b->value.bytes.length;
          int c = n ? memcmp (a->value.bytes.data, b->value.bytes.data, n) : 0;
          return c < 0   ? -1
                 : c > 0 ? 1
                         : (a->value.bytes.length > b->value.bytes.length)
                               - (a->value.bytes.length < b->value.bytes.length);
        }
      default:
        return 2;
      }
  }
#ifdef __cplusplus
}
#endif
#endif
