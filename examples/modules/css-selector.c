/* Complete, offline css-selector example. */
#define MULIB_IMPL_CSS_SELECTOR
#define MULIB_IMPL_DOM_QUERY
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_HTML_TOKENIZER
#define MULIB_IMPL_HTML_TREEBUILDER
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-css-selector.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_html_tree_node_t *root = NULL;
  const char *html = "<p class='a featured'>x</p>";
  assert (mulib_html_tree_parse (html, strlen (html), 10, 5, &root));
  assert (mulib_css_selector_matches (root->first_child, ".featured"));
  assert (mulib_css_selector_specificity (".featured") == 10);
  mulib_html_tree_free (root);
  return 0;
}
