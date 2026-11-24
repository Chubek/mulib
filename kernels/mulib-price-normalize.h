/** @file mulib-price-normalize.h
 * @brief Checked tax, discount, shipping and per-unit offer normalization.
 * C11 single-header module. Define MULIB_IMPL_PRICE_NORMALIZE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_NORMALIZE_H
#define MULIB_PRICE_NORMALIZE_H
#include "mulib-money.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* Discount 0..10000 bp, tax 0..100000 bp; quantity is positive. */
  typedef struct mulib_price_normalize_input
  {
    mulib_money_t base, shipping;
    uint32_t discount_bp, tax_bp, quantity;
    int base_includes_tax;
  } mulib_price_normalize_input_t;
  /** @brief See manual/mulib-price-normalize.md for this operation's contract.
   */
  int mulib_price_normalize (const mulib_price_normalize_input_t *in,
                             mulib_money_t *total, mulib_money_t *per_unit);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_NORMALIZE_H */
#ifdef MULIB_IMPL_PRICE_NORMALIZE
#ifndef MULIB_IMPL_PRICE_NORMALIZE_ONCE
#define MULIB_IMPL_PRICE_NORMALIZE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_normalize (const mulib_price_normalize_input_t *in,
                       mulib_money_t *total, mulib_money_t *unit)
{
  if (!in || !total || !unit || total == unit || !in->quantity
      || in->discount_bp > 10000 || in->tax_bp > 100000 || in->base.minor < 0
      || in->shipping.minor < 0)
    return 0;
  mulib_money_t v;
  if (!mulib_money_scale (in->base, 10000 - (int64_t)in->discount_bp, 10000,
                          &v))
    return 0;
  if (!in->base_includes_tax
      && !mulib_money_scale (v, 10000 + (int64_t)in->tax_bp, 10000, &v))
    return 0;
  if (!mulib_money_add (v, in->shipping, &v))
    return 0;
  if (!mulib_money_scale (v, 1, in->quantity, unit))
    return 0;
  *total = v;
  return 1;
}
#endif
#endif
