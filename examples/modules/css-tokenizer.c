/* Complete, offline css-tokenizer example. */
#define MULIB_IMPL_CSS_TOKENIZER
#include "mulib-css-tokenizer.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int
on_css (void *ctx, const mulib_css_token_t *t)
{
  assert (t->length);
  (*(size_t *)ctx)++;
  return 1;
}

int
main (void)
{
  const char *css = "p { width: 12px; }";
  size_t count = 0;
  assert (mulib_css_tokenizer_scan (css, strlen (css), on_css, &count, 100)
          && count > 0);
  return 0;
}
