#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_HTML_TOKENIZER
#define MULIB_IMPL_HTML_TREEBUILDER
#define MULIB_IMPL_BOUNDED_TEXT
#define MULIB_IMPL_UTF8
#include "mulib-html-treebuilder.h"
#include <assert.h>
#include <string.h>

static mulib_html_tree_node_t *
child (mulib_html_tree_node_t *parent, const char *tag)
{
  mulib_html_tree_node_t *n = parent->first_child;
  assert (n && n->parent == parent && !strcmp (n->tag, tag));
  return n;
}

static mulib_html_tree_node_t *
peer (mulib_html_tree_node_t *prev, const char *tag)
{
  mulib_html_tree_node_t *n = prev->next_sibling;
  assert (n && n->parent == prev->parent && !strcmp (n->tag, tag));
  return n;
}

int
main (void)
{
  const char list[] = "<ul><li><b>A<li data-k=x>&amp;</ul><p>B";
  mulib_html_tree_node_t *root = NULL;
  assert (mulib_html_tree_parse (list, sizeof list - 1, 8, 3, &root));
  mulib_html_tree_node_t *ul = child (root, "ul");
  mulib_html_tree_node_t *first = child (ul, "li");
  mulib_html_tree_node_t *second = peer (first, "li");
  assert (child (child (first, "b"), "")->is_text);
  assert (second == ul->last_child && !second->next_sibling);
  assert (second->offset == (size_t)(strstr (list, "<li data") - list));
  assert (second->attributes.len == sizeof "<li data-k=x>" - 1);
  assert (!memcmp (second->attributes.data, "<li data-k=x>", second->attributes.len));
  mulib_html_tree_node_t *text = child (second, "");
  assert (text->is_text && text->text.len == 5 && !memcmp (text->text.data, "&amp;", 5));
  assert (peer (ul, "p") == root->last_child);
  mulib_html_tree_node_t *unchanged = root;
  assert (!mulib_html_tree_parse (list, sizeof list - 1, 7, 3, &unchanged));
  assert (unchanged == root);
  assert (!mulib_html_tree_parse (list, sizeof list - 1, 8, 2, &unchanged));
  assert (unchanged == root);
  mulib_html_tree_free (root);

  const char nested[] = "<ol><li><ul><li>x</ul><li>y</ol>";
  assert (mulib_html_tree_parse (nested, sizeof nested - 1, 7, 4, &root));
  first = child (child (root, "ol"), "li");
  assert (child (child (first, "ul"), "li"));
  assert (peer (first, "li"));
  mulib_html_tree_free (root);

  const char groups[] = "<dl><dt><em>a<dd>b</dl>"
                        "<select><optgroup><option>a<option>b<optgroup><option>c</select>"
                        "<p><i>d<p>e";
  assert (mulib_html_tree_parse (groups, sizeof groups - 1, 32, 3, &root));
  mulib_html_tree_node_t *dl = child (root, "dl");
  assert (peer (child (dl, "dt"), "dd"));
  mulib_html_tree_node_t *select = peer (dl, "select");
  first = child (select, "optgroup");
  assert (peer (child (first, "option"), "option") == first->last_child);
  assert (child (peer (first, "optgroup"), "option"));
  first = peer (select, "p");
  assert (child (first, "i") && peer (first, "p"));
  mulib_html_tree_free (root);

  const char isolated[] = "<p><template><p>x</p></template><p>y";
  assert (mulib_html_tree_parse (isolated, sizeof isolated - 1, 8, 3, &root));
  first = child (root, "p");
  assert (child (child (first, "template"), "p"));
  assert (peer (first, "p"));
  mulib_html_tree_free (root);

  const char table[] = "<table><thead><tr><th><i>a<th>b"
                       "<tbody><tr><td>c<tr><td>d</table>";
  assert (mulib_html_tree_parse (table, sizeof table - 1, 16, 5, &root));
  mulib_html_tree_node_t *outer = child (root, "table");
  mulib_html_tree_node_t *head = child (outer, "thead");
  first = child (child (head, "tr"), "th");
  assert (child (first, "i") && peer (first, "th"));
  mulib_html_tree_node_t *body = peer (head, "tbody");
  first = child (body, "tr");
  assert (child (first, "td") && child (peer (first, "tr"), "td"));
  mulib_html_tree_free (root);

  const char nested_table[] = "<table><tr><td><table><tr><td>x<td>y"
                              "</table><td>z</table>";
  assert (mulib_html_tree_parse (nested_table, sizeof nested_table - 1, 12, 6, &root));
  first = child (child (child (root, "table"), "tr"), "td");
  second = child (child (child (first, "table"), "tr"), "td");
  assert (peer (second, "td") && peer (first, "td"));
  mulib_html_tree_free (root);
  return 0;
}
