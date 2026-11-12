/** @file mulib-webextract-rules.h
 * @brief Executable bounded HTML element/tag/class/id text extraction rules.
 * C11 single-header module. Define MULIB_IMPL_WEBEXTRACT_RULES in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_WEBEXTRACT_RULES_H
#define MULIB_WEBEXTRACT_RULES_H
#include "mulib-dom-query.h"
#include "mulib-html-textstream.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_webextract_rule
  {
    char selector[128];
    size_t max_bytes;
  } mulib_webextract_rule_t;
  /** @brief See manual/mulib-webextract-rules.md for this operation's
   * contract. */
  int mulib_webextract_rules_text (const char *html, size_t n,
                                   const mulib_webextract_rule_t *rule,
                                   mulib_shylock_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBEXTRACT_RULES_H */
#ifdef MULIB_IMPL_WEBEXTRACT_RULES
#ifndef MULIB_IMPL_WEBEXTRACT_RULES_ONCE
#define MULIB_IMPL_WEBEXTRACT_RULES_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
mulib_webextract_rules_text_ (const mulib_html_tree_node_t *n,
                              mulib_shylock_buffer_t *b)
{
  for (; n; n = n->next_sibling)
    {
      if (n->is_text)
        {
          if (!mulib_shylock_append (b, n->text.data, n->text.len))
            return 0;
        }
      else if (strcmp (n->tag, "script") && strcmp (n->tag, "style")
               && n->first_child
               && !mulib_webextract_rules_text_ (n->first_child, b))
        return 0;
    }
  return 1;
}
int
mulib_webextract_rules_text (const char *h, size_t n,
                             const mulib_webextract_rule_t *r,
                             mulib_shylock_buffer_t *out)
{
  if (!r || !out || !r->selector[0]
      || !memchr (r->selector, 0, sizeof r->selector))
    return 0;
  mulib_html_tree_node_t *root = NULL;
  if (!mulib_html_tree_parse (h, n, 100000, 128, &root))
    return 0;
  const mulib_html_tree_node_t *node
      = mulib_dom_query_first (root, r->selector, 100001);
  mulib_shylock_buffer_t b = { 0 };
  b.limit = r->max_bytes ? r->max_bytes : 65536;
  int ok = node && mulib_webextract_rules_text_ (node->first_child, &b)
           && mulib_shylock_append (out, b.data, b.len);
  mulib_shylock_buffer_free (&b);
  mulib_html_tree_free (root);
  return ok;
}
#endif
#endif
