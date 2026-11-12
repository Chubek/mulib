/* Complete, offline css-cascade example. */
#define MULIB_IMPL_CSS_CASCADE
#define MULIB_IMPL_CSS_COMPUTED
#define MULIB_IMPL_CSS_PARSER
#define MULIB_IMPL_CSS_SELECTOR
#define MULIB_IMPL_DOM_QUERY
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_HTML_TOKENIZER
#define MULIB_IMPL_HTML_TREEBUILDER
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-css-cascade.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_html_tree_node_t *root = NULL;
  mulib_css_parsed_sheet_t sheet = { 0 };
  mulib_css_computed_t style;
  assert (mulib_html_tree_parse ("<p>x</p>", 8, 10, 5, &root));
  const char *css = "p { color: red; }";
  assert (mulib_css_parse_sheet (css, strlen (css), 10, &sheet));
  assert (mulib_css_cascade_compute (&sheet, root->first_child, NULL, &style)
          && !strcmp (style.color, "red"));
  mulib_css_sheet_free (&sheet);
  mulib_html_tree_free (root);
  return 0;
}
