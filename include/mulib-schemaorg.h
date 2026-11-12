/** @file mulib-schemaorg.h
 * @brief Bounded extraction of plain Product/Offer JSON-LD scalar fields.
 * C11 single-header module. Define MULIB_IMPL_SCHEMAORG in exactly one C
 * translation unit. Supports a flat/scalar extraction profile; not a complete
 * JSON-LD or Schema.org processor.
 */
#ifndef MULIB_SCHEMAORG_H
#define MULIB_SCHEMAORG_H
#include "mulib-json.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_schemaorg_offer
  {
    char name[256], currency[8], price[48], availability[128];
  } mulib_schemaorg_offer_t;
  /** @brief See manual/mulib-schemaorg.md for this operation's contract. */
  int mulib_schemaorg_offer_parse (const char *json, size_t n,
                                   mulib_schemaorg_offer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SCHEMAORG_H */
#ifdef MULIB_IMPL_SCHEMAORG
#ifndef MULIB_IMPL_SCHEMAORG_ONCE
#define MULIB_IMPL_SCHEMAORG_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
mulib_schemaorg_text_ (const mulib_json_t *o, const char *key, char *out,
                       size_t cap, int required)
{
  const mulib_json_t *v = mulib_json_get (o, key);
  if (!v)
    return !required;
  size_t n = 0;
  const char *text = mulib_json_text (v, &n);
  if (!text || n >= cap || memchr (text, 0, n))
    return 0;
  memcpy (out, text, n);
  out[n] = 0;
  return !required || n;
}
int
mulib_schemaorg_offer_parse (const char *s, size_t n,
                             mulib_schemaorg_offer_t *out)
{
  if (!s || !out || n > 4 * 1024 * 1024)
    return 0;
  mulib_json_t *doc = mulib_json_parse (s, n, NULL);
  if (!doc)
    return 0;
  const mulib_json_t *offer = mulib_json_get (doc, "offers");
  if (!offer)
    offer = doc;
  mulib_schemaorg_offer_t z = { 0 };
  int ok = mulib_json_typeof (doc) == MULIB_JSON_OBJECT
           && mulib_json_typeof (offer) == MULIB_JSON_OBJECT;
  if (ok)
    ok = mulib_schemaorg_text_ (doc, "name", z.name, sizeof z.name, 0)
         && mulib_schemaorg_text_ (offer, "priceCurrency", z.currency,
                                   sizeof z.currency, 1)
         && mulib_schemaorg_text_ (offer, "availability", z.availability,
                                   sizeof z.availability, 0);
  const mulib_json_t *price = mulib_json_get (offer, "price");
  if (ok)
    {
      if (mulib_json_typeof (price) == MULIB_JSON_NUMBER)
        {
          int k = snprintf (z.price, sizeof z.price, "%.17g",
                            mulib_json_numeric (price));
          ok = k > 0 && (size_t)k < sizeof z.price;
        }
      else
        ok = mulib_schemaorg_text_ (offer, "price", z.price, sizeof z.price,
                                    1);
    }
  double v;
  if (ok)
    ok = strlen (z.currency) == 3 && z.currency[0] >= 'A'
         && z.currency[0] <= 'Z' && z.currency[1] >= 'A'
         && z.currency[1] <= 'Z' && z.currency[2] >= 'A'
         && z.currency[2] <= 'Z' && mulib_shylock_parse_decimal (z.price, &v)
         && v >= 0;
  mulib_json_destroy (doc);
  if (ok)
    *out = z;
  return ok;
}
#endif
#endif
