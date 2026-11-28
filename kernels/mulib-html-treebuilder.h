/** @file mulib-html-treebuilder.h
 * @brief Owned bounded HTML element/text tree with basic optional-end-tag
 * recovery. C11 single-header module. Define MULIB_IMPL_HTML_TREEBUILDER in
 * exactly one C translation unit. Not a full WHATWG HTML5 tree builder;
 * supported subset is explicitly bounded.
 */
#ifndef MULIB_HTML_TREEBUILDER_H
#define MULIB_HTML_TREEBUILDER_H
#include "mulib-html-tokenizer.h"
#include "mulib-bounded-text.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_html_tree_node
  {
    char tag[64];
    mulib_bounded_text_buffer_t text, attributes;
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
mulib_html_tree_section_ (const char *tag)
{
  return !strcmp (tag, "thead") || !strcmp (tag, "tbody")
         || !strcmp (tag, "tfoot");
}

static int
mulib_html_tree_cell_ (const char *tag)
{
  return !strcmp (tag, "td") || !strcmp (tag, "th");
}

/* Recover peers within explicit table markup, without inventing containers.
   Commit the pop only after finding the owning table, never past a template. */
static void
mulib_html_tree_table_end_ (mulib_html_tree_state_ *s, const char *tag)
{
  int cell = mulib_html_tree_cell_ (tag);
  int row = !strcmp (tag, "tr");
  int section = mulib_html_tree_section_ (tag);
  if (!cell && !row && !section)
    return;

  mulib_html_tree_node_t *close = NULL;
  size_t depth = s->depth, close_depth = 0;
  for (mulib_html_tree_node_t *n = s->active; n != s->root;
       n = n->parent, depth--)
    {
      if (!strcmp (n->tag, "template"))
        return;
      if (!strcmp (n->tag, "table"))
        {
          if (close)
            {
              s->active = close->parent;
              s->depth = close_depth - 1;
            }
          return;
        }
      if (!strcmp (n->tag, "tr"))
        {
          if ((row || section) && !close)
            close = n, close_depth = depth;
          cell = 0;
        }
      else if (mulib_html_tree_section_ (n->tag))
        {
          if (section)
            close = n, close_depth = depth;
          cell = row = section = 0;
        }
      else if (cell && !close && mulib_html_tree_cell_ (n->tag))
        close = n, close_depth = depth;
    }
}

/* Only handle optional-end-tag recovery in the supported in-body subset.
   Do not pop an enclosing list, select group or an isolated subtree. */
static void
mulib_html_tree_imply_end_ (mulib_html_tree_state_ *s, const char *tag)
{
  int li = !strcmp (tag, "li");
  int dl = !strcmp (tag, "dt") || !strcmp (tag, "dd");
  int p = !strcmp (tag, "p");
  int group = !strcmp (tag, "optgroup");
  int option = group || !strcmp (tag, "option");
  if (!li && !dl && !p && !option)
    return;

  size_t depth = s->depth;
  for (mulib_html_tree_node_t *n = s->active; n != s->root;
       n = n->parent, depth--)
    {
      const char *open = n->tag;
      if ((li && !strcmp (open, "li"))
          || (dl && (!strcmp (open, "dt") || !strcmp (open, "dd")))
          || (p && !strcmp (open, "p"))
          || (option && !strcmp (open, "option"))
          || (group && !strcmp (open, "optgroup")))
        {
          s->active = n->parent;
          s->depth = depth - 1;
          if (group && !strcmp (open, "option")
              && !strcmp (s->active->tag, "optgroup"))
            {
              s->active = s->active->parent;
              s->depth--;
            }
          return;
        }
      if ((option && !strcmp (open, "optgroup"))
          || !strcmp (open, "html") || !strcmp (open, "table")
          || !strcmp (open, "td") || !strcmp (open, "th")
          || !strcmp (open, "template") || !strcmp (open, "select")
          || !strcmp (open, "button")
          || (li && (!strcmp (open, "ul") || !strcmp (open, "ol")
                     || !strcmp (open, "menu")))
          || (dl && !strcmp (open, "dl")))
        return;
    }
}

static int
mulib_html_tree_cb_ (void *p, const mulib_html_token_t *t)
{
  mulib_html_tree_state_ *s = p;
  if (t->kind == MULIB_HTML_STREAM_COMMENT)
    return 1;
  if (t->kind == MULIB_HTML_STREAM_END)
    {
      size_t depth = s->depth;
      for (mulib_html_tree_node_t *n = s->active; n != s->root;
           n = n->parent, depth--)
        if (!strcmp (n->tag, t->name))
          {
            s->active = n->parent;
            s->depth = depth - 1;
            break;
          }
      return 1;
    }
  if (s->nodes >= s->max_nodes)
    return 0;
  if (t->kind == MULIB_HTML_STREAM_START)
    {
      mulib_html_tree_imply_end_ (s, t->name);
      mulib_html_tree_table_end_ (s, t->name);
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
      if (!mulib_bounded_text_append (&n->text, t->raw, t->length))
        {
          free (n);
          return 0;
        }
    }
  else
    {
      memcpy (n->tag, t->name, sizeof n->tag);
      if (!mulib_bounded_text_append (&n->attributes, t->raw, t->length))
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
  mulib_html_tree_state_ st = { root, root, 0, max_nodes, 0, max_depth };
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
      mulib_bounded_text_buffer_free (&root->text);
      mulib_bounded_text_buffer_free (&root->attributes);
      free (root);
      root = next;
    }
}
#endif
#endif
