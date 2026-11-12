#include "fixtures/web_primitives_impl.h"
#include <assert.h>
static int
html (void *p, const mulib_html_token_t *t)
{
  (void)p;
  (void)t;
  return 1;
}
static int
css (void *p, const mulib_css_token_t *t)
{
  (void)p;
  (void)t;
  return 1;
}
static int
ld (void *p, mulib_shylock_span_t s, size_t o)
{
  (void)p;
  (void)s;
  (void)o;
  return 1;
}
int
main (void)
{
  uint32_t seed = 23;
  char bytes[257];
  const char alphabet[] = "<>/!*{}:;='#\"\\ abcdef0123456789\n\t";
  for (size_t trial = 0; trial < 2000; trial++)
    {
      size_t n = trial % 257;
      for (size_t i = 0; i < n; i++)
        {
          seed = seed * 1664525u + 1013904223u;
          bytes[i] = alphabet[seed % (sizeof alphabet - 1)];
        }
      bytes[n] = 0;
      mulib_html_tree_node_t *root = NULL;
      (void)mulib_html_tree_parse (bytes, n, 512, 32, &root);
      mulib_html_tree_free (root);
      (void)mulib_html_tokenizer_scan (bytes, n, html, NULL, 512);
      (void)mulib_html_jsonld_scan (bytes, n, ld, NULL, 10);
      mulib_shylock_buffer_t b = { 0 };
      b.limit = 512;
      (void)mulib_readability_extract (bytes, n, &b);
      mulib_shylock_buffer_free (&b);
      mulib_css_parsed_sheet_t sheet = { 0 };
      (void)mulib_css_parse_sheet (bytes, n, 20, &sheet);
      mulib_css_sheet_free (&sheet);
      (void)mulib_css_tokenizer_scan (bytes, n, css, NULL, 512);
      mulib_css_computed_t computed;
      assert (mulib_css_computed_defaults (&computed));
      (void)mulib_css_computed_apply (&computed, bytes, n);
      (void)mulib_webidl_bindgen_interfaces (bytes, n, &b, 10);
      mulib_shylock_buffer_free (&b);
      mulib_browser_origin_t origin;
      (void)mulib_browser_origin_parse (bytes, &origin);
      mulib_schemaorg_offer_t offer;
      (void)mulib_schemaorg_offer_parse (bytes, n, &offer);
    }
  return 0;
}
