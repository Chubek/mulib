/** @file mulib-html-treebuilder.h
 * @brief Owned bounded HTML element/text tree with basic optional-end-tag
 * recovery. C11 single-header module. Define MULIB_IMPL_HTML_TREEBUILDER in
 * exactly one C translation unit. Not a full WHATWG HTML5 tree builder;
 * supported subset is explicitly bounded.
 */
#ifndef MULIB_HTML_TREEBUILDER_H
#define MULIB_HTML_TREEBUILDER_H
#include "mulib-html-tokenizer.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_html_tree_node
  {
    char tag[64];
    mulib_shylock_buffer_t text, attributes;
    struct mulib_html_tree_node *parent, *first_child, *last_child,
        *next_sibling;
    size_t offset;
    int is_text;
  } mulib_html_tree_node_t;
  /** @brief See manual/mulib-html-treebuilder.md for this operation's
   * contract. */
  int mulib_html_tree_parse (const char *html, size_t n, size_t max_nodes,
                             size_t max_depth, mulib_html_tree_node_t **out);
  /** @brief See manual/mulib-html-treebuilder.md for this operation's
   * contract. */
  void mulib_html_tree_free (mulib_html_tree_node_t *root);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_TREEBUILDER_H */
#ifdef MULIB_IMPL_HTML_TREEBUILDER
#ifndef MULIB_IMPL_HTML_TREEBUILDER_ONCE
#define MULIB_IMPL_HTML_TREEBUILDER_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mulib_html_tree_state_
{
  mulib_html_tree_node_t *root, *active;
  size_t nodes, max_nodes, depth, max_depth;
  int failed;
} mulib_html_tree_state_;
static int
mulib_html_tree_void_ (const char *s)
{
  const char *a[]
      = { "area",  "base", "br",   "col",   "embed",  "hr",    "img",
          "input", "link", "meta", "param", "source", "track", "wbr" };
  for (size_t i = 0; i < sizeof a / sizeof *a; i++)
    if (!strcmp (s, a[i]))
      return 1;
  return 0;
}
static int
mulib_html_tree_cb_ (void *p, const mulib_html_token_t *t)
{
  mulib_html_tree_state_ *s = p;
  if (t->kind == MULIB_HTML_STREAM_COMMENT)
    return 1;
  if (t->kind == MULIB_HTML_STREAM_END)
    {
      for (mulib_html_tree_node_t *n = s->active; n && n != s->root;
           n = n->parent)
        if (!strcmp (n->tag, t->name))
          {
            s->active = n->parent;
            size_t depth = 0;
            for (n = s->active; n && n != s->root; n = n->parent)
              depth++;
            s->depth = depth;
            break;
          }
      return 1;
    }
  if (s->nodes >= s->max_nodes)
    return 0;
  if (t->kind == MULIB_HTML_STREAM_START && s->active != s->root
      && !strcmp (s->active->tag, t->name)
      && (!strcmp (t->name, "li") || !strcmp (t->name, "p")
          || !strcmp (t->name, "option")))
    {
      s->active = s->active->parent;
      if (s->depth)
        s->depth--;
    }
  if (t->kind == MULIB_HTML_STREAM_START
      && (!t->name[0] || s->depth >= s->max_depth))
    return 0;
  mulib_html_tree_node_t *n = calloc (1, sizeof *n);
  if (!n)
    return 0;
  n->offset = t->offset;
  n->is_text = t->kind == MULIB_HTML_STREAM_TEXT;
  if (n->is_text)
    {
      if (!mulib_shylock_append (&n->text, t->raw, t->length))
        {
          free (n);
          return 0;
        }
    }
  else
    {
      memcpy (n->tag, t->name, sizeof n->tag);
      if (!mulib_shylock_append (&n->attributes, t->raw, t->length))
        {
          free (n);
          return 0;
        }
    }
  n->parent = s->active;
  if (s->active->last_child)
    s->active->last_child->next_sibling = n;
  else
    s->active->first_child = n;
  s->active->last_child = n;
  s->nodes++;
  if (t->kind == MULIB_HTML_STREAM_START && !mulib_html_tree_void_ (n->tag)
      && !(t->length >= 2 && t->raw[t->length - 2] == '/'))
    {
      s->active = n;
      s->depth++;
    }
  return 1;
}
int
mulib_html_tree_parse (const char *html, size_t n, size_t max_nodes,
                       size_t max_depth, mulib_html_tree_node_t **out)
{
  if (!out || (!html && n) || !max_nodes || !max_depth || max_depth > 1024
      || max_nodes > (SIZE_MAX - 10) / 2)
    return 0;
  mulib_html_tree_node_t *root = calloc (1, sizeof *root);
  if (!root)
    return 0;
  strcpy (root->tag, "#document");
  mulib_html_tree_state_ st = { root, root, 0, max_nodes, 0, max_depth, 0 };
  if (!mulib_html_tokenizer_scan (html, n, mulib_html_tree_cb_, &st,
                                  max_nodes * 2 + 10))
    {
      mulib_html_tree_free (root);
      return 0;
    }
  *out = root;
  return 1;
}
void
mulib_html_tree_free (mulib_html_tree_node_t *root)
{
  while (root)
    {
      mulib_html_tree_node_t *next = root->next_sibling;
      mulib_html_tree_free (root->first_child);
      mulib_shylock_buffer_free (&root->text);
      mulib_shylock_buffer_free (&root->attributes);
      free (root);
      root = next;
    }
}
#endif
#endif
