/** @file mulib-dom-query.h
 * @brief Bounded tag, ID, and class lookups over owned HTML tree nodes.
 * C11 single-header module. Define MULIB_IMPL_DOM_QUERY in exactly one C
 * translation unit. Supports single simple selectors only, not full Selectors
 * Level 4.
 */
#ifndef MULIB_DOM_QUERY_H
#define MULIB_DOM_QUERY_H
#include "mulib-html-treebuilder.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  const mulib_html_tree_node_t *
  /** @brief See manual/mulib-dom-query.md for this operation's contract. */
  mulib_dom_query_first (const mulib_html_tree_node_t *root,
                         const char *selector, size_t max_nodes);
  /** @brief Match a single type, #id, .class or * selector; no combinators. */
  int mulib_dom_query_matches (const mulib_html_tree_node_t *node,
                               const char *selector);
  /** @brief See manual/mulib-dom-query.md for this operation's contract. */
  size_t mulib_dom_query_all (const mulib_html_tree_node_t *root,
                              const char *selector,
                              const mulib_html_tree_node_t **out, size_t cap,
                              size_t max_nodes);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOM_QUERY_H */
#ifdef MULIB_IMPL_DOM_QUERY
#ifndef MULIB_IMPL_DOM_QUERY_ONCE
#define MULIB_IMPL_DOM_QUERY_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_dom_query_matches (const mulib_html_tree_node_t *n, const char *s)
{
  if (!n || n->is_text || n->tag[0] == '#' || !s || !*s)
    return 0;
  if (!strcmp (s, "*"))
    return 1;
  const char *word = (*s == '.' || *s == '#') ? s + 1 : s;
  if (!*word)
    return 0;
  for (const char *p = word; *p; p++)
    if (!isalnum ((unsigned char)*p) && *p != '-' && *p != '_' && *p != ':')
      return 0;
  if (s[0] != '.' && s[0] != '#')
    return mulib_shylock_equal_ci (s, n->tag);
  const char *v;
  size_t len, z = strlen (word);
  if (!mulib_html_attribute (n->attributes.data, n->attributes.len,
                             s[0] == '#' ? "id" : "class", &v, &len))
    return 0;
  if (s[0] == '#')
    return len == z && !memcmp (v, word, z);
  for (size_t i = 0; i < len;)
    {
      while (i < len && isspace ((unsigned char)v[i]))
        i++;
      size_t start = i;
      while (i < len && !isspace ((unsigned char)v[i]))
        i++;
      if (i - start == z && !memcmp (v + start, word, z))
        return 1;
    }
  return 0;
}
static void
mulib_dom_query_walk_ (const mulib_html_tree_node_t *n, const char *s,
                       const mulib_html_tree_node_t **out, size_t cap,
                       size_t *seen, size_t *matches, size_t max)
{
  for (; n && *seen < max; n = n->next_sibling)
    {
      (*seen)++;
      if (mulib_dom_query_matches (n, s))
        {
          if (out && *matches < cap)
            out[*matches] = n;
          (*matches)++;
        }
      if (n->first_child)
        mulib_dom_query_walk_ (n->first_child, s, out, cap, seen, matches,
                               max);
    }
}
size_t
mulib_dom_query_all (const mulib_html_tree_node_t *root, const char *selector,
                     const mulib_html_tree_node_t **out, size_t cap,
                     size_t max)
{
  if (!root || !selector || !max)
    return 0;
  size_t seen = 0, matches = 0;
  mulib_dom_query_walk_ (root, selector, out, cap, &seen, &matches, max);
  return matches;
}
const mulib_html_tree_node_t *
mulib_dom_query_first (const mulib_html_tree_node_t *root,
                       const char *selector, size_t limit)
{
  const mulib_html_tree_node_t *n = NULL;
  return mulib_dom_query_all (root, selector, &n, 1, limit) ? n : NULL;
}
#endif
#endif
