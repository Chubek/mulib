/* Complete, offline html-jsonld example. */
#define MULIB_IMPL_HTML_JSONLD
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_HTML_TOKENIZER
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-html-jsonld.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int
on_json (void *ctx, mulib_shylock_span_t span, size_t offset)
{
  assert (offset > 0 && span.len == 2);
  (*(size_t *)ctx)++;
  return 1;
}

int
main (void)
{
  const char *html = "<script type='application/ld+json'>{}</script>";
  size_t count = 0;
  assert (mulib_html_jsonld_scan (html, strlen (html), on_json, &count, 10)
          && count == 1);
  return 0;
}
