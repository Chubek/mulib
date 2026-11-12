/** @file mulib-browser-documentload.h
 * @brief Synchronous document fetch and HTML tree creation orchestration.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_DOCUMENTLOAD in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_DOCUMENTLOAD_H
#define MULIB_BROWSER_DOCUMENTLOAD_H
#include "mulib-browser-fetch.h"
#include "mulib-html-treebuilder.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-browser-documentload.md for this operation's
   * contract. */
  int mulib_browser_documentload (const mulib_browser_fetch_t *fetch,
                                  const char *url, size_t max_nodes,
                                  mulib_html_tree_node_t **root,
                                  unsigned *status);
  /** @brief Load with explicit tree depth and response byte budgets. */
  int mulib_browser_documentload_bounded (const mulib_browser_fetch_t *fetch,
                                          const char *url, size_t max_nodes,
                                          size_t max_depth, size_t max_bytes,
                                          mulib_html_tree_node_t **root,
                                          unsigned *status);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_DOCUMENTLOAD_H */
#ifdef MULIB_IMPL_BROWSER_DOCUMENTLOAD
#ifndef MULIB_IMPL_BROWSER_DOCUMENTLOAD_ONCE
#define MULIB_IMPL_BROWSER_DOCUMENTLOAD_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_documentload_bounded (const mulib_browser_fetch_t *f,
                                    const char *url, size_t max, size_t depth,
                                    size_t bytes,
                                    mulib_html_tree_node_t **root,
                                    unsigned *status)
{
  if (!f || !url || !root || !status || !max || !depth || depth > 1024
      || !bytes)
    return 0;
  mulib_browser_fetch_t fetch = *f;
  if (!fetch.client.policy.max_bytes || fetch.client.policy.max_bytes > bytes)
    fetch.client.policy.max_bytes = bytes;
  mulib_shylock_buffer_t body = { 0 };
  body.limit = bytes;
  char type[128] = { 0 };
  unsigned code = 0;
  int ok
      = mulib_browser_fetch_get (&fetch, url, &body, &code, type, sizeof type);
  if (ok)
    {
      char media[128];
      size_t n = 0;
      while (type[n] && type[n] != ';' && !isspace ((unsigned char)type[n]))
        {
          media[n] = (char)tolower ((unsigned char)type[n]);
          n++;
        }
      media[n] = 0;
      size_t end = n;
      while (isspace ((unsigned char)type[end]))
        end++;
      ok = code >= 200 && code < 300 && (!type[end] || type[end] == ';')
           && !strcmp (media, "text/html")
           && mulib_html_tree_parse (body.data, body.len, max, depth, root);
    }
  if (ok)
    *status = code;
  mulib_shylock_buffer_free (&body);
  return ok;
}
int
mulib_browser_documentload (const mulib_browser_fetch_t *f, const char *url,
                            size_t max, mulib_html_tree_node_t **root,
                            unsigned *status)
{
  return mulib_browser_documentload_bounded (f, url, max, 128,
                                             16 * 1024 * 1024, root, status);
}
#endif
#endif
