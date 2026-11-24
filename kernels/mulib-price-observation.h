/** @file mulib-price-observation.h
 * @brief Validated price, product, merchant, market, time and evidence record.
 * C11 single-header module. Define MULIB_IMPL_PRICE_OBSERVATION in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_PRICE_OBSERVATION_H
#define MULIB_PRICE_OBSERVATION_H
#include "mulib-money.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_observation
  {
    char product[128], merchant[128], variant[64], url[512], market[16];
    mulib_money_t price;
    int64_t observed_at;
    uint64_t source_hash;
    unsigned
        flags; /* bit 0: in stock; bit 1: promotion; bit 2: includes tax */
  } mulib_price_observation_t;
  /** @brief See manual/mulib-price-observation.md for this operation's
   * contract. */
  int mulib_price_observation_validate (const mulib_price_observation_t *o);
  /** @brief See manual/mulib-price-observation.md for this operation's
   * contract. */
  int mulib_price_observation_same_offer (const mulib_price_observation_t *a,
                                          const mulib_price_observation_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_OBSERVATION_H */
#ifdef MULIB_IMPL_PRICE_OBSERVATION
#ifndef MULIB_IMPL_PRICE_OBSERVATION_ONCE
#define MULIB_IMPL_PRICE_OBSERVATION_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_observation_validate (const mulib_price_observation_t *o)
{
  if (!o || !memchr (o->product, 0, sizeof o->product) || !o->product[0]
      || !memchr (o->merchant, 0, sizeof o->merchant) || !o->merchant[0]
      || !memchr (o->variant, 0, sizeof o->variant)
      || !memchr (o->url, 0, sizeof o->url)
      || !memchr (o->market, 0, sizeof o->market) || o->price.minor < 0
      || o->price.decimals > 6 || !memchr (o->price.currency, 0, 4)
      || strlen (o->price.currency) != 3 || o->observed_at < 0)
    return 0;
  return mulib_money_valid (o->price) && !(o->flags & ~7u);
}
int
mulib_price_observation_same_offer (const mulib_price_observation_t *a,
                                    const mulib_price_observation_t *b)
{
  return a && b && mulib_price_observation_validate (a)
         && mulib_price_observation_validate (b)
         && !strcmp (a->product, b->product)
         && !strcmp (a->merchant, b->merchant)
         && !strcmp (a->variant, b->variant) && !strcmp (a->market, b->market)
         && !strcmp (a->price.currency, b->price.currency)
         && a->price.decimals == b->price.decimals;
}
#endif
#endif
