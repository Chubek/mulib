/** @file mulib-css-selector.h
 * @brief Simple type, ID, class and universal selector matching for HTML tree
 * nodes. C11 single-header module. Define MULIB_IMPL_CSS_SELECTOR in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_CSS_SELECTOR_H
#define MULIB_CSS_SELECTOR_H
#include "mulib-dom-query.h"
#include "mulib-html-treebuilder.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-css-selector.md for this operation's contract. */
  int mulib_css_selector_matches (const mulib_html_tree_node_t *node,
                                  const char *selector);
  /** @brief See manual/mulib-css-selector.md for this operation's contract. */
  int mulib_css_selector_specificity (const char *selector);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_SELECTOR_H */
#ifdef MULIB_IMPL_CSS_SELECTOR
#ifndef MULIB_IMPL_CSS_SELECTOR_ONCE
#define MULIB_IMPL_CSS_SELECTOR_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_css_selector_matches (const mulib_html_tree_node_t *n, const char *sel)
{
  return mulib_dom_query_matches (n, sel);
}
int
mulib_css_selector_specificity (const char *s)
{
  if (!s || !*s)
    return -1;
  if (!strcmp (s, "*"))
    return 0;
  const char *p = (*s == '#' || *s == '.') ? s + 1 : s;
  if (!*p)
    return -1;
  for (; *p; p++)
    if (!isalnum ((unsigned char)*p) && *p != '-' && *p != '_' && *p != ':')
      return -1;
  return *s == '#' ? 100 : *s == '.' ? 10 : 1;
}
#endif
#endif
