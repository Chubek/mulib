/* Complete, offline schemaorg example. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_SCHEMAORG
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-schemaorg.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  const char *json = "{\"price\":\"12.30\",\"priceCurrency\":\"USD\"}";
  mulib_schemaorg_offer_t offer;
  assert (mulib_schemaorg_offer_parse (json, strlen (json), &offer));
  assert (!strcmp (offer.price, "12.30"));
  return 0;
}
