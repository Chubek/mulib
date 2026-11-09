#define MULIB_IMPL_WEBEXTRACT
#include "mulib-webextract.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_webextract_span_t x = { 0 };
  assert (mulib_webextract_jsonld (
      "<script type=\"application/ld+json\">{}</script>", &x));
  assert (x.length == 2);
  return 0;
}
