/** @file mulib-price-history.h
 * @brief Owned ordered and deduplicated price observation history.
 * C11 single-header module. Define MULIB_IMPL_PRICE_HISTORY in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_HISTORY_H
#define MULIB_PRICE_HISTORY_H
#include "mulib-price-observation.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_history
  {
    mulib_price_observation_t *items;
    size_t len, cap, limit;
  } mulib_price_history_t;
  /** @brief See manual/mulib-price-history.md for this operation's contract.
   */
  int mulib_price_history_add (mulib_price_history_t *h,
                               const mulib_price_observation_t *o);
  /** @brief See manual/mulib-price-history.md for this operation's contract.
   */
  size_t mulib_price_history_range (const mulib_price_history_t *h,
                                    int64_t from, int64_t to,
                                    mulib_price_observation_t *out,
                                    size_t cap);
  /** @brief See manual/mulib-price-history.md for this operation's contract.
   */
  void mulib_price_history_free (mulib_price_history_t *h);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_HISTORY_H */
#ifdef MULIB_IMPL_PRICE_HISTORY
#ifndef MULIB_IMPL_PRICE_HISTORY_ONCE
#define MULIB_IMPL_PRICE_HISTORY_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_history_add (mulib_price_history_t *h,
                         const mulib_price_observation_t *o)
{
  if (!h || !mulib_price_observation_validate (o))
    return 0;
  mulib_price_observation_t value = *o;
  o = &value;
  size_t p = 0;
  while (p < h->len && h->items[p].observed_at < o->observed_at)
    p++;
  for (size_t j = p; j < h->len && h->items[j].observed_at == o->observed_at;
       j++)
    if (mulib_price_observation_same_offer (h->items + j, o))
      {
        h->items[j] = *o;
        return 1;
      }
  if (h->limit && h->len >= h->limit)
    return 0;
  if (h->len == h->cap)
    {
      size_t c = h->cap ? h->cap * 2 : 32;
      if (c < h->cap || c > SIZE_MAX / sizeof *h->items)
        return 0;
      if (h->limit && c > h->limit)
        c = h->limit;
      void *t = realloc (h->items, c * sizeof *h->items);
      if (!t)
        return 0;
      h->items = t;
      h->cap = c;
    }
  memmove (h->items + p + 1, h->items + p, (h->len - p) * sizeof *h->items);
  h->items[p] = *o;
  h->len++;
  return 1;
}
size_t
mulib_price_history_range (const mulib_price_history_t *h, int64_t a,
                           int64_t b, mulib_price_observation_t *out,
                           size_t cap)
{
  if (!h || a > b)
    return 0;
  size_t n = 0;
  for (size_t i = 0; i < h->len; i++)
    if (h->items[i].observed_at >= a && h->items[i].observed_at <= b)
      {
        if (out && n < cap)
          out[n] = h->items[i];
        n++;
      }
  return n;
}
void
mulib_price_history_free (mulib_price_history_t *h)
{
  if (h)
    {
      free (h->items);
      memset (h, 0, sizeof *h);
    }
}
#endif
#endif
