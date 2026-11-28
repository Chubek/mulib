/** @file mulib-html-dombridge.h
 * @brief Callback tree walker and opt-in bounded native HTML document parser.
 * C11 single-header module. Define MULIB_IMPL_HTML_DOMBRIDGE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_HTML_DOMBRIDGE_H
#define MULIB_HTML_DOMBRIDGE_H
#include "mulib-html-treebuilder.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_html_dombridge_ops
  {
    void *(*make) (void *ctx, const mulib_html_tree_node_t *node,
                   void *parent);
    int (*text) (void *ctx, void *node, const char *text, size_t length);
  } mulib_html_dombridge_ops_t;
  /** @brief See manual/mulib-html-dombridge.md for this operation's contract.
   */
  int mulib_html_dombridge_walk (const mulib_html_tree_node_t *root,
                                  const mulib_html_dombridge_ops_t *ops,
                                  void *ctx, void *parent, size_t depth_limit);
  typedef struct mulib_dom_document mulib_dom_document_t;
  typedef struct mulib_html_dombridge_limits
  {
    size_t max_bytes, max_nodes, max_depth;
  } mulib_html_dombridge_limits_t;
  typedef enum mulib_html_dombridge_status
  {
    MULIB_HTML_DOMBRIDGE_OK,
    MULIB_HTML_DOMBRIDGE_INVALID_ARGUMENT,
    MULIB_HTML_DOMBRIDGE_INVALID_INPUT,
    MULIB_HTML_DOMBRIDGE_UNSUPPORTED,
    MULIB_HTML_DOMBRIDGE_LIMIT,
    MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY
  } mulib_html_dombridge_status_t;
  /** Requires MULIB_IMPL_HTML_DOMBRIDGE_NATIVE. Publishes a new owned native
   * document only on success; neither walks nor builds the legacy HTML tree.
   * See manual/html-native-dom-profile.md for grammar and resource limits. */
  mulib_html_dombridge_status_t
  mulib_html_dombridge_parse (const char *html, size_t length,
                              const mulib_html_dombridge_limits_t *limits,
                              mulib_dom_document_t **out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_DOMBRIDGE_H */
#ifdef MULIB_IMPL_HTML_DOMBRIDGE
#ifndef MULIB_IMPL_HTML_DOMBRIDGE_ONCE
#define MULIB_IMPL_HTML_DOMBRIDGE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_html_dombridge_walk (const mulib_html_tree_node_t *root,
                           const mulib_html_dombridge_ops_t *ops, void *ctx,
                           void *parent, size_t max)
{
  if (!ops || !ops->make || !root || !max)
    return 0;
  for (const mulib_html_tree_node_t *n = root; n; n = n->next_sibling)
    {
      void *node = ops->make (ctx, n, parent);
      if (!node)
        return 0;
      if (n->is_text && ops->text
          && !ops->text (ctx, node, n->text.data, n->text.len))
        return 0;
      if (n->first_child
          && !mulib_html_dombridge_walk (n->first_child, ops, ctx, node,
                                         max - 1))
        return 0;
    }
  return 1;
}
#endif
#endif
#ifdef MULIB_IMPL_HTML_DOMBRIDGE_NATIVE
#ifndef MULIB_IMPL_HTML_DOMBRIDGE_NATIVE_ONCE
#define MULIB_IMPL_HTML_DOMBRIDGE_NATIVE_ONCE
#include "mulib-dom.h"
#include "detail/mulib-html-native-document.h"
#endif
#endif
