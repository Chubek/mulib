/* Complete, offline webextract-rules example. */
#define MULIB_IMPL_DOM_QUERY
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_HTML_TOKENIZER
#define MULIB_IMPL_HTML_TREEBUILDER
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_WEBEXTRACT_RULES
#include "mulib-webextract-rules.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_webextract_rule_t rule = { ".price", 128 };
  mulib_shylock_buffer_t text = { 0 };
  const char *html = "<span class='sale price'>12.30</span>";
  assert (mulib_webextract_rules_text (html, strlen (html), &rule, &text));
  assert (!strcmp (text.data, "12.30"));
  mulib_shylock_buffer_free (&text);
  return 0;
}
