/* Complete, offline html-treebuilder example. */
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_HTML_TOKENIZER
#define MULIB_IMPL_HTML_TREEBUILDER
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-html-treebuilder.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_html_tree_node_t *root = NULL;
  const char *html = "<p>Hello<br>world</p>";
  assert (mulib_html_tree_parse (html, strlen (html), 100, 10, &root));
  assert (!strcmp (root->first_child->tag, "p"));
  mulib_html_tree_free (root);
  return 0;
}
