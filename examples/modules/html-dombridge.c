/* Complete, offline html-dombridge example. */
#define MULIB_IMPL_HTML_DOMBRIDGE
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_HTML_TOKENIZER
#define MULIB_IMPL_HTML_TREEBUILDER
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-html-dombridge.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static void *
make (void *ctx, const mulib_html_tree_node_t *node, void *parent)
{
  (void)node;
  (void)parent;
  (*(size_t *)ctx)++;
  return ctx;
}

int
main (void)
{
  mulib_html_tree_node_t *root = NULL;
  size_t count = 0;
  mulib_html_dombridge_ops_t ops = { make, NULL };
  assert (mulib_html_tree_parse ("<p>x</p>", 8, 10, 5, &root));
  assert (mulib_html_dombridge_walk (root, &ops, &count, NULL, 10)
          && count == 3);
  mulib_html_tree_free (root);
  return 0;
}
