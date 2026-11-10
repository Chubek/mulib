/* MuLIB: Owned growable batch of deep-copied records. C11 single-header; define
 * MULIB_IMPL_RECORDBATCH once. */
#ifndef MULIB_RECORDBATCH_H
#define MULIB_RECORDBATCH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-record.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    mulib_record_t *items;
    size_t count, capacity;
  } mulib_recordbatch_t;
  int mulib_recordbatch_append (mulib_recordbatch_t *b, const mulib_record_t *r);
  void mulib_recordbatch_free (mulib_recordbatch_t *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RECORDBATCH) && !defined(MULIB_RECORDBATCH_IMPLEMENTED)
#define MULIB_RECORDBATCH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_recordbatch_append (mulib_recordbatch_t *b, const mulib_record_t *r)
  {
    if (!b || !r)
      return 0;
    mulib_record_t copy = { 0 };
    if (!mulib_record_clone (&copy, r))
      return 0;
    if (b->count == b->capacity)
      {
        size_t cap = b->capacity ? b->capacity * 2 : 8;
        if (cap < b->capacity || cap > SIZE_MAX / sizeof (*b->items))
          {
            mulib_record_free (&copy);
            return 0;
          }
        void *p = realloc (b->items, cap * sizeof (*b->items));
        if (!p)
          {
            mulib_record_free (&copy);
            return 0;
          }
        b->items = (mulib_record_t *)p;
        b->capacity = cap;
      }
    b->items[b->count++] = copy;
    return 1;
  }
  void
  mulib_recordbatch_free (mulib_recordbatch_t *b)
  {
    if (!b)
      return;
    for (size_t i = 0; i < b->count; i++)
      mulib_record_free (&b->items[i]);
    free (b->items);
    memset (b, 0, sizeof (*b));
  }
#ifdef __cplusplus
}
#endif
#endif
