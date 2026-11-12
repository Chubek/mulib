/* Complete, offline html-tokenizer example. */
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_HTML_TOKENIZER
#include "mulib-html-tokenizer.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  const char *tag = "<p CLASS = 'a b'>", *value;
  size_t length;
  assert (mulib_html_attribute (tag, strlen (tag), "class", &value, &length));
  assert (length == 3 && !memcmp (value, "a b", 3));
  return 0;
}
